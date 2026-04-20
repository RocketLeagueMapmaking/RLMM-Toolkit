#include "Updater.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPushButton>

#include "RTQSplash.h"

Updater::Updater(QNetworkAccessManager& networkManager, QObject* parent)
    : QObject(parent), mNam(networkManager)
{
}

void Updater::checkForUpdate(const QUrl& releaseUrl)
{
    QNetworkRequest request{ releaseUrl };
    request.setRawHeader(QByteArray("Accept"), QByteArray("application/vnd.github+json"));
    request.setRawHeader(QByteArray("User-Agent"), QByteArray("RLMM-Toolkit"));

    QNetworkReply* reply = mNam.get(request);
    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit failed("Network Error");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit failed("Invalid JSON");
            return;
        }

        QString tagName = doc.object().value("tag_name").toString();
        if (tagName.isEmpty()) {
            emit failed("Invalid tag");
            return;
        }

        QString latest = tagName.startsWith('v') ? tagName.mid(1) : tagName;

        QVersionNumber currentVer = QVersionNumber::fromString(VERSION_APP);
        QVersionNumber latestVer = QVersionNumber::fromString(latest);

        if (latestVer > currentVer)
            emit updateAvailable(latest);
        else
            emit upToDate();
        });

    QObject::connect(this, &Updater::updateAvailable, [&](const QString& ver) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::NoIcon);
        msgBox.setWindowTitle("Update Available");
        msgBox.setText(QString("Version %1 is available.").arg(ver));

        QPushButton* updateBtn = msgBox.addButton(tr("Update"), QMessageBox::AcceptRole);
        msgBox.addButton(tr("Skip"), QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.clickedButton() == updateBtn) {
            startUpdate(QUrl(RELEASE_URL), RELEASE_SHA);
        }

        loop.quit();
        });

    QObject::connect(this, &Updater::upToDate, [&]() {
        loop.quit();
        });

    QObject::connect(this, &Updater::failed, [&](const QString& err) {
        Q_UNUSED(err);
        loop.quit();
        });

    loop.exec();
}

void Updater::startUpdate(const QUrl& assetUrl, const QByteArray& sha)
{
    mExpectedSha256Hex = sha;
    mDownloadPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath("RLMMToolkit-Update.zip");

    qDebug(mDownloadPath.toStdString().c_str());

    mDownloadFile = new QFile(mDownloadPath, this);
    if (!mDownloadFile->open(QIODevice::WriteOnly)) {
        emit failed("Cannot open temp file: " + mDownloadFile->errorString());
        return;
    }

    QNetworkRequest request(assetUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);

    mReply = mNam.get(request);
    connect(mReply, &QNetworkReply::downloadProgress, this, &Updater::progress);
    connect(mReply, &QNetworkReply::readyRead, this, [this]() { mDownloadFile->write(mReply->readAll()); });
    connect(mReply, &QNetworkReply::finished, this, &Updater::onFinished);
}

void Updater::onFinished()
{
    mDownloadFile->write(mReply->readAll());
    mDownloadFile->close();

    const bool    ok = mReply->error() == QNetworkReply::NoError;
    const QString err = mReply->errorString();
    mReply->deleteLater();
    mReply = nullptr;

    if (!ok) { emit failed("Download failed: " + err); return; }
    if (!verifyChecksum()) { emit failed("Checksum mismatch");        return; }

    launchHelperAndExit();
}

bool Updater::verifyChecksum() const
{
    QFile file(mDownloadPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    if (!hasher.addData(&file)) return false;

    return hasher.result().toHex() == mExpectedSha256Hex;
}

void Updater::launchHelperAndExit()
{
    const QString helper = QCoreApplication::applicationDirPath() + "/RLMMUpdater.exe";
    const QStringList args = {
        QString::number(QCoreApplication::applicationPid()),
        mDownloadPath,
        QCoreApplication::applicationFilePath()
    };
    QProcess::startDetached(helper, args);
    QCoreApplication::quit();
}