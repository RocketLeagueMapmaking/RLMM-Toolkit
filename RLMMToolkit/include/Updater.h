#pragma once

#include <QNetworkAccessManager>
#include <QFile>
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include "version.h"

class Updater : public QObject {
	Q_OBJECT

public:
	explicit Updater(QNetworkAccessManager& networkManager, QObject* parent = nullptr);

	void checkForUpdate(const QUrl& releaseUrl);
	void startUpdate(const QUrl& assetUrl, const QByteArray& expectedSha256Hex);

signals:
	void progress(qint64 bytesReceived, qint64 bytesTotal);
	void failed(const QString& reason);
	void updateAvailable(const QString& version, const QUrl& assetUrl, const QByteArray& sha256Hex);
	void upToDate();

private:
	QUrl mAssetUrl;
	QByteArray mAssetSha;

	QNetworkAccessManager& mNam;
};