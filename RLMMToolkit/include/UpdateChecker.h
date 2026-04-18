#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(const QString& currentVersion, QObject* parent = nullptr);
    void checkForUpdates();

signals:
    void updateAvailable(const QString& newVersion);
    void upToDate();
    void checkFailed();

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QString m_currentVersion;
    QNetworkAccessManager m_manager;
};