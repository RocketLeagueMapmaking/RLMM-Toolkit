#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shlwapi.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "miniz.h"

namespace fs = std::filesystem;

// ---------- logging ----------

static std::ofstream gLog;

static void initLog() {
    wchar_t tempBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tempBuf);
    fs::path logPath = fs::path(tempBuf) / "rlmm_updater.log";
    gLog.open(logPath, std::ios::app);
}

static void log(const std::string& msg) {
    if (!gLog.is_open()) {
        return;
    }
    auto t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    gLog << std::put_time(&tm, "%FT%T") << "  " << msg << "\n";
    gLog.flush();
}

// ---------- string conversion ----------

static std::wstring toWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
    return out;
}

static std::string toUtf8(const std::wstring& s) {
    if (s.empty()) {
        return {};
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len, nullptr, nullptr);
    return out;
}

// ---------- wait for parent ----------

static void waitForProcess(uint32_t pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) {
        return;
    }
    WaitForSingleObject(h, 30000);
    CloseHandle(h);
}

// ---------- HTTP download ----------

static bool downloadFile(const std::wstring& url, const fs::path& dest) {
    // Parse URL into components.
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};

    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        log("WinHttpCrackUrl failed");
        return false;
    }

    HINTERNET session = WinHttpOpen(L"RLMM-Toolkit-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return false;
    }

    HINTERNET conn = WinHttpConnect(session, host, uc.nPort, 0);
    if (!conn) {
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return false;
    }

    // Auto-follow redirects (default behavior, but be explicit).
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

    bool ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(req, nullptr);

    if (ok) {
        std::ofstream out(dest, std::ios::binary);
        if (!out) {
            ok = false;
        }
        else {
            std::vector<char> buf(64 * 1024);
            DWORD avail = 0;
            while (ok && WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
                DWORD got = 0;
                if (!WinHttpReadData(req, buf.data(), (DWORD)std::min<size_t>(avail, buf.size()), &got)) {
                    ok = false;
                    break;
                }
                out.write(buf.data(), got);
            }
        }
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return ok;
}

// ---------- SHA256 ----------

static std::string sha256Hex(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }

    std::vector<char> buf(64 * 1024);
    while (f) {
        f.read(buf.data(), buf.size());
        auto gc = f.gcount();
        if (gc > 0) {
            BCryptHashData(hash, (PUCHAR)buf.data(), (ULONG)gc, 0);
        }
    }

    unsigned char digest[32];
    BCryptFinishHash(hash, digest, sizeof(digest), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char b : digest) hex << std::setw(2) << (int)b;
    return hex.str();
}

// ---------- zip extract (miniz) ----------

static bool extractZip(const fs::path& zipPath, const fs::path& destDir) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, zipPath.string().c_str(), 0)) {
        return false;
    }

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) {
            mz_zip_reader_end(&zip);
            return false;
        }

        fs::path outPath = destDir / st.m_filename;
        if (st.m_is_directory) {
            fs::create_directories(outPath);
            continue;
        }

        fs::create_directories(outPath.parent_path());
        if (!mz_zip_reader_extract_to_file(&zip, i, outPath.string().c_str(), 0)) {
            mz_zip_reader_end(&zip);
            return false;
        }
    }
    mz_zip_reader_end(&zip);
    return true;
}

// ---------- copy tree with retry ----------

static bool copyTreeWithRetry(const fs::path& srcDir, const fs::path& dstDir) {
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
    fs::path selfName = fs::path(selfPath).filename();
    
    for (auto& entry : fs::recursive_directory_iterator(srcDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename() == selfName) {
            continue;
        }

        fs::path rel = fs::relative(entry.path(), srcDir);
        fs::path dst = dstDir / rel;
        fs::create_directories(dst.parent_path());

        bool ok = false;
        for (int i = 0; i < 20 && !ok; ++i) {
            std::error_code ec;
            fs::remove(dst, ec);
            fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
            ok = !ec;
            if (!ok) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        if (!ok) {
            log("copy failed: " + entry.path().string());
            return false;
        }
    }
    return true;
}

// ---------- main ----------

int wmain(int argc, wchar_t** argv) {
    initLog();

    if (argc < 5) {
        log("FAIL: insufficient args");
        return 1;
    }

    const uint32_t parentPid = (uint32_t)std::stoul(argv[1]);
    const std::wstring url = argv[2];
    const std::string expectedSha = toUtf8(argv[3]);
    const fs::path appExe = argv[4];
    const fs::path installDir = appExe.parent_path();
    const fs::path stagingDir = installDir / "_update_staging";

    wchar_t tempBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tempBuf);
    const fs::path zipPath = fs::path(tempBuf) / "RLMMToolkit-Update.zip";

    log("argc=" + std::to_string(argc));
    for (int i = 0; i < argc; ++i) {
        log("argv[" + std::to_string(i) + "]=" + toUtf8(argv[i]));
    }

    waitForProcess(parentPid);

    if (!downloadFile(url, zipPath)) {
        log("FAIL: download");
        return 2;
    }

    if (sha256Hex(zipPath) != expectedSha) {
        log("FAIL: checksum");
        return 3;
    }

    std::error_code ec;
    fs::remove_all(stagingDir, ec);
    fs::create_directories(stagingDir);

    if (!extractZip(zipPath, stagingDir)) {
        log("FAIL: extract");
        return 4;
    }

    if (!copyTreeWithRetry(stagingDir, installDir)) {
        log("FAIL: copy");
        return 5;
    }

    fs::remove_all(stagingDir, ec);
    fs::remove(zipPath, ec);

    // Relaunch the new GUI.
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + appExe.wstring() + L"\"";
    CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
        0, nullptr, installDir.wstring().c_str(), &si, &pi);
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    log("Done");
    return 0;
}