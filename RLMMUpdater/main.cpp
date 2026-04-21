#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QThread>
#include "miniz.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

static void waitForProcess(qint64 pid) {
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!h) return;
    WaitForSingleObject(h, 30000);
    CloseHandle(h);
#else
    for (int i = 0; i < 60 && ::kill(static_cast<pid_t>(pid), 0) == 0; ++i)
        QThread::msleep(500);
#endif
}

static bool extractZip(const QString& zipPath, const QString& destDir) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, zipPath.toUtf8().constData(), 0)) return false;

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) { mz_zip_reader_end(&zip); return false; }

        const QString outPath = QDir(destDir).filePath(QString::fromUtf8(st.m_filename));
        if (st.m_is_directory) { QDir().mkpath(outPath); continue; }

        QDir().mkpath(QFileInfo(outPath).absolutePath());
        if (!mz_zip_reader_extract_to_file(&zip, i, outPath.toUtf8().constData(), 0)) {
            mz_zip_reader_end(&zip); return false;
        }
    }
    mz_zip_reader_end(&zip);
    return true;
}

static bool copyTreeWithRetry(const QString& srcDir, const QString& dstDir) {
    QDirIterator it(srcDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString src = it.next();
        const QString rel = QDir(srcDir).relativeFilePath(src);
        const QString dst = QDir(dstDir).filePath(rel);
        QDir().mkpath(QFileInfo(dst).absolutePath());

        bool ok = false;
        for (int i = 0; i < 20 && !ok; ++i) {
            QFile::remove(dst);
            ok = QFile::copy(src, dst);
            if (!ok) QThread::msleep(250);
        }
        if (!ok) return false;
    }
    return true;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 4) return 1;

    const qint64 parentPid = args[1].toLongLong();
    const QString zipPath = args[2];
    const QString appExePath = args[3];
    const QString installDir = QFileInfo(appExePath).absolutePath();
    const QString staging = QDir(installDir).filePath("_update_staging");

    waitForProcess(parentPid);

    QDir(staging).removeRecursively();
    QDir().mkpath(staging);

    if (!extractZip(zipPath, staging)) return 2;
    if (!copyTreeWithRetry(staging, installDir)) return 3;

    QDir(staging).removeRecursively();
    QFile::remove(zipPath);

    QProcess::startDetached(appExePath, {});
    return 0;
}