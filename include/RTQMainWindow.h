#pragma once
#include <QWidget>
#include <QMainWindow>
#include <QTabBar>
#include <QStackedWidget>


class RTQMainWindow : public QMainWindow {
Q_OBJECT

public:
    RTQMainWindow(QWidget* parent = nullptr);

private:
    void onTabChanged(int index);

    QTabBar* mTabBar;
    QTabWidget* mHomeTab;
};
