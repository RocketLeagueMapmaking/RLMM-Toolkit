#pragma once
#include <QSplashScreen>

#include <QApplication>
#include <QNetworkAccessManager>
#include <QWidget>
#include <QPixmap>
#include <QQueue>
#include <QString>

#include <functional>

#include "Updater.h"

class RTQInitTask : public QObject {
    Q_OBJECT
public:
	RTQInitTask(const QString& message, std::function<void(RTQInitTask&)> func, QObject* parent = nullptr);
	
	void execute() { mFunc(*this); }
	void complete(bool success) { emit finished(success); };
	
	QString message;

signals:
	void finished(bool success);

private:
	std::function<void(RTQInitTask&)> mFunc;
};


class RTQSplash : public QSplashScreen {
Q_OBJECT
public:
	RTQSplash(QApplication& app, Updater& updater);
	~RTQSplash();

	bool run();
	void enqueueTask(const QString& message, std::function<void(RTQInitTask&)> func);

protected:
	void drawContents(QPainter* painter) override;

private:
	void initialize(RTQInitTask& task);
	void checkForUpdates(RTQInitTask& task);

	void onUpdateAvailable(RTQInitTask& task, const QString& ver, const QUrl& url, const QByteArray& sha);
	void onUpToDate(RTQInitTask& task);
	void onUpdateFailed(RTQInitTask& task, const QString& err);

	QApplication& mApp;
	Updater& mUpdater;

	QPixmap mLogo;
	QFont mConfigFont;

	QQueue<RTQInitTask*> mInitTasks;
};


