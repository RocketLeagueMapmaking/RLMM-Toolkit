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
#include <QJsonArray>
#include <QVersionNumber>
#include <QTimer>

Updater::Updater(QNetworkAccessManager& networkManager, QObject* parent)
    : QObject(parent), mNam(networkManager)
{
}

void Updater::checkForUpdate(const QUrl& releaseUrl) {
    QNetworkRequest request{ releaseUrl };
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "RLMM-Toolkit");

    QNetworkReply* reply = mNam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit failed("Network Error");
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = root.value("tag_name").toString();
        if (tag.isEmpty()) {
            emit failed("Invalid tag");
            return;
        }

        QUrl assetUrl;
        QByteArray assetSha;
        for (const QJsonValue& v : root.value("assets").toArray()) {
            const QJsonObject asset = v.toObject();
            if (asset.value("name").toString().endsWith(".zip")) {
                assetUrl = QUrl(asset.value("browser_download_url").toString());
                assetSha = asset.value("digest").toString().section(':', 1).toUtf8();
                break;
            }
        }
        if (assetUrl.isEmpty() || assetSha.isEmpty()) {
            emit failed("Missing release asset");
            return;
        }

        const QString clean = tag.startsWith('v') ? tag.mid(1) : tag;
        if (QVersionNumber::fromString(clean) > QVersionNumber::fromString(VERSION_APP)) {
            emit updateAvailable(clean, assetUrl, assetSha);
        }
        else {
            emit upToDate();
        }
        });
}

void Updater::startUpdate(const QUrl& assetUrl, const QByteArray& sha) {
    const QString helper = QCoreApplication::applicationDirPath() + "/RLMMUpdater.exe";
    const QStringList args = {
      QString::number(QCoreApplication::applicationPid()),
      assetUrl.toString(),
      QString::fromUtf8(sha),
      QCoreApplication::applicationFilePath()
    };

    qint64 pid = 0;
    const bool ok = QProcess::startDetached(helper, args, QString(), &pid);
    if (!ok) { emit failed("Failed to launch updater"); return; }

    QTimer::singleShot(100, qApp, &QCoreApplication::quit);
}
