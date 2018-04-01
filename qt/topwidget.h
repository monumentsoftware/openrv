#ifndef TOPWIDGET_H
#define TOPWIDGET_H

#include <QWidget>

#include <libopenrv/libopenrv.h>
#include <libopenrv/orv_latencytesterclient.h>

class ServerListModel;
class ServerListWidget;
class NewServerWidget;
class ConnectStateWidget;
class OrvWidget;
class QStackedLayout;
class QTimer;
struct orv_context_t;
class OrvContext;

/**
 * Toplevel widget that is the central widget of the @ref MainWindow.
 *
 * This widget displays (depending on the current connection state) either
 * - a @ref ConnectWidget for the user to enter the host to connect to, or
 * - a @ref ConnectStateWidget displaying the current connection progress or the result of an
 *   aborted connection, or
 * - a @ref OrvWidget, the actual OpenRV content.
 **/
class TopWidget : public QWidget
{
    Q_OBJECT
public:
    TopWidget(QWidget* parent);
    virtual ~TopWidget();

    void setInitialHostName(const QString& hostName);
    void setInitialPort(uint16_t port);
    void setInitialPassword(const QString& password);
    void startConnect();

public slots:
    void showConnectionInfo();
    void startConnectToLatencyTester();
    void disconnectFromLatencyTester();
    void startLatencyTest();
    void stopLatencyTest();
    void addLatencyTesterData(const QRect& previousRect, const QRect& newRect, qint64 clientSendTimeUs, qint64 clientReceiveTimeUs);
    void setDebugBitPlanes(int planes);

signals:
    void canAbortConnectionUpdated(bool canAbort);
    void canAbortConnectionToLatencyTesterUpdated(bool canAbort);
    void orvContextChanged(struct orv_context_t* context);

protected:
    enum class WidgetType
    {
        ServerList,
        NewServer,
        ConnectStateOrResult,
        OpenRV,
    };
    struct LatencyTesterData
    {
        QRect mPreviousRect;
        QRect mNewRect;
        qint64 mClientSendTimeUs = 0;
        qint64 mClientReceiveTimeUs = 0;
    };
protected:
    void changeCurrentWidgetTo(WidgetType w);
    void handleLatencyTesterFramebufferUpdate(const orv_event_framebuffer_t* data, qint64 eventReceiveUs, qint64 updatedUs);
    void handleLatencyData(const LatencyTesterData& data, qint64 orvEventReceiveUs, qint64 orvUpdateUs);

protected slots:
    void handleConnectEvent(const orv_connect_result_t* data);
    void handleDisconnectedEvent(const orv_disconnected_t* data);
    void handleCutTextEvent(const QString& text);
    void handleFramebufferUpdatedEvent(const orv_event_framebuffer_t* data);
    void handleFramebufferUpdateRequestFinishedEvent();
    void handleCursorUpdatedEvent();
    void handleBellEvent();
    void abortConnect();
    void requestFramebufferUpdate();
    void connectionInfoDestroyed();
    void saveToServerList(const QString& host, int port, const QString& password, const QString& name, bool savePassword, int internalServerId, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat);
    void connectToHost(const QModelIndex& index);
    void connectToHost(const QString& host, int port, const QString& password, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat, int internalServerId = -1);
    void editServer(const QModelIndex& index);
    void cancelNewServer();
    void deleteServerAndCancelNewServer(int internalServerId);
    void showAddNewServerWidget();
    void showServerListWidget();
    void sendCanAbortConnectionToLatencyTesterUpdated(bool canAbortConnection);
    void latencyTestTick();

private:
    ServerListModel* mServerListModel = nullptr;
    QStackedLayout* mLayout = nullptr;
    ServerListWidget* mServerListWidget = nullptr;
    NewServerWidget* mNewServerWidget = nullptr;
    ConnectStateWidget* mConnectStateWidget = nullptr;
    OrvWidget* mOrvWidget = nullptr;
    OrvContext* mQtOrvContext = nullptr;
    QDialog* mConnectionInfoDialog = nullptr;
    bool mWasDisconnectedWaitForUser = false;
    struct orv_latency_tester_client_t* mLatencyTesterClient = nullptr;
    QTimer* mLatencyTestTimer = nullptr;
    QList<LatencyTesterData> mLatencyTesterData;
};

#endif

