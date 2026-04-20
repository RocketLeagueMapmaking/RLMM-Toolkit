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

class RTQInitTask {
public:
	explicit RTQInitTask(const QString& message, std::function<bool()> func);
	bool execute() { return mFunc(); }
	QString message;

private:
	std::function<bool()> mFunc;
};


class RTQSplash : public QSplashScreen {
Q_OBJECT
public:
	RTQSplash(QApplication& app, Updater& updater);
	~RTQSplash();

	bool run();

protected:
	void drawContents(QPainter* painter) override;

private:
	bool initialize();
	bool checkForUpdates();

	QApplication& mApp;
	Updater& mUpdater;

	QPixmap mLogo;
	QFont mConfigFont;

	QQueue<RTQInitTask> mInitTasks;
};


