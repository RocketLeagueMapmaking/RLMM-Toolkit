#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QSplashScreen>
#include <QMessageBox>

#include <chrono>
#include <thread>

#include "RTQMainWindow.h"
#include "RTQSplash.h"
#include "Updater.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QNetworkAccessManager nam;

    Updater updater(nam);
    RTQSplash splash(app, updater);
    RTQMainWindow window(nullptr);

    splash.show();
    splash.run();
    window.show();
    splash.finish(&window);
    return app.exec();
}