#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QThread>

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
    for (int i = 0; i < 60 && ::kill(static_cast<pid_t>(pid), 0) == 0; ++i) {
        QThread::msleep(500);
    }
#endif
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 4) return 1;

    const qint64 parentPid = args[1].toLongLong();
    const QString source = args[2];
    const QString target = args[3];

    waitForProcess(parentPid);

    QFile::remove(target);
    if (!QFile::rename(source, target)) return 2;

    QProcess::startDetached(target, {});
    return 0;
}