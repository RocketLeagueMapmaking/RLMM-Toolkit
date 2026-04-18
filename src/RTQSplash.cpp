#include "RTQSplash.h"

#include <thread>

#include <QWidget>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPainter>
#include <QFontDatabase>

#include "filepaths.h"
#include "version.h"


RTQSplash::RTQSplash(QApplication& app) :
    app(app),
    QSplashScreen(QPixmap(PATH_SPLASH))
{
    mLogo = QPixmap(PATH_LOGO).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int fontId = QFontDatabase::addApplicationFont(PATH_FONT_CONFIG);
    QString family = QFontDatabase::applicationFontFamilies(fontId).first();
    mConfigFont = QFont(family, 24);

    show();

    mInitTasks.enqueue(RTQInitTask("Initializing...", [&]() -> bool {return initialize();}));
    mInitTasks.enqueue(RTQInitTask("Checking for updates...", [&]() -> bool {return checkForUpdates();}));

    showMessage(VERSION_APP, Qt::AlignTop | Qt::AlignLeft, Qt::white);
}

RTQSplash::~RTQSplash() {}

void RTQSplash::drawContents(QPainter* painter) {
    QSplashScreen::drawContents(painter);

    // Logo
    painter->drawPixmap(8, 8, mLogo);

    // Custom font text
    painter->setFont(mConfigFont);
    painter->setPen(Qt::white);
    painter->drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignBottom | Qt::AlignLeft, "RLMM Toolkit");

    painter->setFont(QFont());
    painter->drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignBottom | Qt::AlignCenter, VERSION_APP);
}

bool RTQSplash::run() {
    while (!mInitTasks.isEmpty()) {
        RTQInitTask currentTask = mInitTasks.dequeue();
        showMessage(currentTask.message, Qt::AlignBottom | Qt::AlignRight, Qt::white);
        app.processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (currentTask.execute()) {
            showMessage("Complete", Qt::AlignBottom | Qt::AlignRight, Qt::white);
        }
        else {
            showMessage("Failed", Qt::AlignBottom | Qt::AlignRight, Qt::white);
        }
    }
    return true;
}

RTQInitTask::RTQInitTask(const QString& message, std::function<bool()> func) :
	message(message), mFunc(func) {

}

bool RTQSplash::initialize() {
    return true;
}

bool RTQSplash::checkForUpdates() {
    QNetworkReply* reply = mNam.get(QNetworkRequest(QUrl(VERSION_URL)));
    bool result = false;
    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed();
            return;
        }
        const QString latest = QString(reply->readAll()).trimmed();
        if (latest > VERSION_APP)
            emit updateAvailable(latest);
        else
            emit upToDate();
        });

    QObject::connect(this, &RTQSplash::updateAvailable, [&](const QString& ver) {
        QMessageBox::information(this, "Update Available",
            QString("Version %1 is available.").arg(ver));
        result = true;
        loop.quit();
        });

    QObject::connect(this, &RTQSplash::upToDate, [&]() {
        result = true;
        loop.quit();
        });

    QObject::connect(this, &RTQSplash::checkFailed, [&]() {
        loop.quit();
        });

    loop.exec();
    return result;
}