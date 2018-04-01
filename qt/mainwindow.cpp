#include "mainwindow.h"
#include "topwidget.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtCore/QTimer>

MainWindow::MainWindow()
    : QMainWindow()
{
    mTopWidget = new TopWidget(this);
    connect(mTopWidget, SIGNAL(canAbortConnectionUpdated(bool)), this, SLOT(canAbortConnectionUpdated(bool)));
    connect(mTopWidget, SIGNAL(canAbortConnectionToLatencyTesterUpdated(bool)), this, SLOT(canAbortConnectionToLatencyTesterUpdated(bool)));
    setCentralWidget(mTopWidget);
    QMenuBar* menuBar = this->menuBar();

    QAction* quit = new QAction(tr("&Quit"), this); // NOTE: won't show up on OSX
    connect(quit, SIGNAL(triggered()), qApp, SLOT(quit()));
    mShowConnectionInfo = new QAction(tr("Show &Connection Info"), this);
    connect(mShowConnectionInfo, SIGNAL(triggered()), mTopWidget, SLOT(showConnectionInfo()));
    mDisconnect = new QAction(tr("&Disconnect"), this);
    mDisconnect->setEnabled(false);
    connect(mDisconnect, SIGNAL(triggered()), mTopWidget, SLOT(abortConnect()));

    mConnectToLatencyTester = new QAction(tr("Connect to Latency Tester"), this);
    connect(mConnectToLatencyTester, SIGNAL(triggered()), mTopWidget, SLOT(startConnectToLatencyTester()));
    mDisconnectFromLatencyTester = new QAction(tr("Disconnect from Latency Tester"), this);
    mDisconnectFromLatencyTester->setEnabled(false);
    connect(mDisconnectFromLatencyTester, SIGNAL(triggered()), mTopWidget, SLOT(disconnectFromLatencyTester()));

    mStartLatencyTest = new QAction(tr("Start latency test"), this);
    mStartLatencyTest->setEnabled(false);
    connect(mStartLatencyTest, SIGNAL(triggered()), this, SLOT(startLatencyTest()));

    mStopLatencyTest = new QAction(tr("Stop latency test"), this);
    mStopLatencyTest->setEnabled(false);
    connect(mStopLatencyTest, SIGNAL(triggered()), this, SLOT(stopLatencyTest()));

    QMenu* fileMenu = menuBar->addMenu(tr("&File"));
    fileMenu->addAction(mShowConnectionInfo);
    fileMenu->addAction(mDisconnect);
    fileMenu->addSeparator();
    fileMenu->addAction(quit);

    QMenu* debugMenu = menuBar->addMenu(tr("&Debug"));
    debugMenu->addAction(mConnectToLatencyTester);
    debugMenu->addAction(mDisconnectFromLatencyTester);
    debugMenu->addAction(mStartLatencyTest);
    debugMenu->addAction(mStopLatencyTest);
    QMenu* bitPlanes = debugMenu->addMenu("BitPlanes");
    for (int i = 0; i < 8; i++) {
        QAction* a = nullptr;
        if (i != 7) {
            a = new QAction(tr("Show only %1 most significant bits per channel").arg(i+1), this);
        }
        else {
            a = new QAction(tr("Show all 8 bit planes per channel"), this);
        }
        connect(a, SIGNAL(triggered()), this, SLOT(debugBitPlanes()));
        a->setEnabled(false);
        mDebugBitPlanes.append(a);
        bitPlanes->addAction(a);
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::setInitialHostName(const QString& hostName)
{
    mTopWidget->setInitialHostName(hostName);
}

void MainWindow::setInitialPort(uint16_t port)
{
    mTopWidget->setInitialPort(port);
}

void MainWindow::setInitialPassword(const QString& password)
{
    mTopWidget->setInitialPassword(password);
}

void MainWindow::startConnect()
{
    mTopWidget->startConnect();
}

void MainWindow::canAbortConnectionUpdated(bool canAbortConnection)
{
    mDisconnect->setEnabled(canAbortConnection);

    // TODO: also disable if connection to vnc server failed, i.e. disable if "connection failed"
    // widget is shown
    mConnectToLatencyTester->setEnabled(canAbortConnection);
    foreach (QAction* a, mDebugBitPlanes) {
        a->setEnabled(canAbortConnection);
    }
}

void MainWindow::canAbortConnectionToLatencyTesterUpdated(bool canAbortConnection)
{
    stopLatencyTest();

    mDisconnectFromLatencyTester->setEnabled(canAbortConnection);
    if (!canAbortConnection) {
        mStartLatencyTest->setEnabled(false);
    }
    else {
        mStartLatencyTest->setEnabled(true);
    }
}

void MainWindow::startLatencyTest()
{
    mStopLatencyTest->setEnabled(true);
    mTopWidget->startLatencyTest();
}

void MainWindow::stopLatencyTest()
{
    mStopLatencyTest->setEnabled(false);
    mTopWidget->stopLatencyTest();
}

void MainWindow::debugBitPlanes()
{
    QAction* a = qobject_cast<QAction*>(sender());
    int index = mDebugBitPlanes.indexOf(a);
    if (index < 0) {
        return;
    }
    mTopWidget->setDebugBitPlanes(index + 1);
}
