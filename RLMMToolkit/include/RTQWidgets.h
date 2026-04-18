#pragma once

#include <QWidget>
#include <QFrame>
#include <QTabWidget>

class RTQTabWidget : public QTabWidget {
Q_OBJECT

public:
	RTQTabWidget(QWidget* parent = nullptr);

private slots:
private:
	std::vector<QFrame> mTabs;
};