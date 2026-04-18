#include <QVBoxLayout>
#include <QLabel>

#include "RTQMainWindow.h"
#include "UpdateChecker.h"

RTQMainWindow::RTQMainWindow(QWidget* parent) : QMainWindow(parent)
{   
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);

    mTabBar = new QTabBar(this);
    mHomeTab = new QTabWidget(this);
    mTabBar->addTab("Tab One");
    mTabBar->addTab("Tab Two");
    mTabBar->addTab("Tab Three");

    layout->addWidget(mTabBar);

    setCentralWidget(central);

    connect(mTabBar, &QTabBar::currentChanged, this, &RTQMainWindow::onTabChanged);
}

void RTQMainWindow::onTabChanged(int index)
{
    //m_stack->setCurrentIndex(index);
}