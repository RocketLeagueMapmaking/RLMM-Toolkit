#include "RTQSplash.h"

#include <thread>

#include <QWidget>
#include <QPainter>
#include <QFontDatabase>
#include <QVersionNumber>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPushButton>

#include "filepaths.h"
#include "version.h"


RTQSplash::RTQSplash(QApplication& app, Updater& updater) :
    mApp(app),
    mUpdater(updater),
    QSplashScreen(QPixmap(PATH_SPLASH))
{
    mLogo = QPixmap(PATH_LOGO).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int fontId = QFontDatabase::addApplicationFont(PATH_FONT_CONFIG);
    QString family = QFontDatabase::applicationFontFamilies(fontId).first();
    mConfigFont = QFont(family, 24);

    enqueueTask("Initializing...", [this](RTQInitTask& t) {return initialize(t);});
    enqueueTask("Checking for updates...", [this](RTQInitTask& t) {return checkForUpdates(t);});

    showMessage(VERSION_APP, Qt::AlignTop | Qt::AlignLeft, Qt::white);
}

RTQSplash::~RTQSplash() {}

void RTQSplash::enqueueTask(const QString& message, std::function<void(RTQInitTask&)> func) {
    mInitTasks.enqueue(new RTQInitTask(message, std::move(func), this));
}

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
    qDebug() << "RUN: entered, queue size:" << mInitTasks.size();
    while (!mInitTasks.isEmpty()) {
        RTQInitTask* currentTask = mInitTasks.dequeue();
        showMessage(currentTask->message, Qt::AlignBottom | Qt::AlignRight, Qt::white);
        mApp.processEvents();

        bool done = false;
        bool success = false;
        connect(currentTask, &RTQInitTask::finished, this, [&](bool ok) {
            qDebug() << "RUN: finished signal received:" << ok;
            success = ok;
            done = true;
            });
        qDebug() << "RUN: calling execute()";
        currentTask->execute();
        qDebug() << "RUN: execute() returned, entering wait loop";

        while (!done) {
            mApp.processEvents(QEventLoop::WaitForMoreEvents);
        }
        qDebug() << "RUN: wait loop exited";

        showMessage(success ? "Complete" : "Failed", Qt::AlignBottom | Qt::AlignRight, Qt::white);
        currentTask->deleteLater();
    }
    qDebug() << "RUN: returning";
    return true;
}

RTQInitTask::RTQInitTask(const QString& message, std::function<void(RTQInitTask&)> func, QObject* parent) :
    QObject(parent), message(message), mFunc(std::move(func)) {

}

void RTQSplash::initialize(RTQInitTask& task) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    task.complete(true);
}

void RTQSplash::checkForUpdates(RTQInitTask& task) {
    connect(&mUpdater, &Updater::updateAvailable, &task,
        [this, &task](const QString& v, const QUrl& u, const QByteArray& s) {
            onUpdateAvailable(task, v, u, s);
        });
    connect(&mUpdater, &Updater::upToDate, &task,
        [this, &task]() { onUpToDate(task); });
    connect(&mUpdater, &Updater::failed, &task,
        [this, &task](const QString& err) { onUpdateFailed(task, err); });

    mUpdater.checkForUpdate(QUrl(VERSION_URL));
}

void RTQSplash::onUpdateAvailable(RTQInitTask& task, const QString& ver, const QUrl& url, const QByteArray& sha) {
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::NoIcon);
    msgBox.setWindowTitle("Update Available");
    msgBox.setText(QString("Version %1 is available.").arg(ver));

    QPushButton* updateBtn = msgBox.addButton(tr("Update"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Skip"), QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() == updateBtn) {
        mUpdater.startUpdate(url, sha);
    }
    task.complete(true);
}

void RTQSplash::onUpToDate(RTQInitTask& task) {
    showMessage("Up to date", Qt::AlignBottom | Qt::AlignRight, Qt::white);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    task.complete(true);
}

void RTQSplash::onUpdateFailed(RTQInitTask& task, const QString& err) {
    qWarning() << "Update check failed:" << err;
    showMessage("Update failed, see log", Qt::AlignBottom | Qt::AlignRight, Qt::white);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    task.complete(false);
}