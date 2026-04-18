#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QSplashScreen>
#include <QMessageBox>

#include <chrono>
#include <thread>

#include "RTQMainWindow.h"
#include "RTQSplash.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    RTQSplash splash(app);
    RTQMainWindow window(nullptr);

    splash.show();
    splash.run();
    window.show();
    splash.finish(&window);
    return app.exec();
}