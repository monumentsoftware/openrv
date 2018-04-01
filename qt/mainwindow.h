#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>

class TopWidget;
class QAction;

/**
 * Main window that provides a menubar and possibly a toolbar and instantiates the @ref TopWidget.
 **/
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();
    virtual ~MainWindow();

    void parseCommandLineArguments(const QStringList& arguments);
    void setInitialHostName(const QString& hostName);
    void setInitialPort(uint16_t port);
    void setInitialPassword(const QString& password);

public slots:
    void startConnect();

protected slots:
    void canAbortConnectionUpdated(bool canAbortConnection);
    void canAbortConnectionToLatencyTesterUpdated(bool canAbortConnection);
    void startLatencyTest();
    void stopLatencyTest();
    void debugBitPlanes();

private:
    TopWidget* mTopWidget = nullptr;
    QAction* mShowConnectionInfo = nullptr;
    QAction* mDisconnect = nullptr;
    QAction* mDisconnectFromLatencyTester = nullptr;
    QAction* mConnectToLatencyTester = nullptr;
    QAction* mStartLatencyTest = nullptr;
    QAction* mStopLatencyTest = nullptr;
    QList<QAction*> mDebugBitPlanes;
};

#endif

