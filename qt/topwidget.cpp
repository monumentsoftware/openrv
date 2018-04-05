/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "topwidget.h"

#include "serverlistwidget.h"
#include "serverlistmodel.h"
#include "newserverwidget.h"
#include "connectstatewidget.h"
#include "orvwidget.h"
#include "connectioninfowidget.h"
#include "orv_context_qt.h"

#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QDialog>
#include <QtCore/QTimer>
#include <QtCore/QtDebug>

#if defined(Q_OS_UNIX)
#include <sys/time.h>
#endif // Q_OS_UNIX

void latencyTesterEventCallback(struct orv_latency_tester_client_t* client, orv_latency_tester_event_type_t event, const void* eventData, void* userData);

TopWidget::TopWidget(QWidget* parent)
    : QWidget(parent)
{
    mServerListModel = new ServerListModel(this);
    mLayout = new QStackedLayout(this);
    mServerListWidget = new ServerListWidget(this);
    mNewServerWidget = new NewServerWidget(this);
    mConnectStateWidget = new ConnectStateWidget(this);
    mOrvWidget = new OrvWidget(this);

    mServerListWidget->setModel(mServerListModel);

    mLayout->addWidget(mNewServerWidget);
    mLayout->addWidget(mServerListWidget);
    mLayout->addWidget(mConnectStateWidget);
    mLayout->addWidget(mOrvWidget);
    mLayout->setCurrentWidget(mServerListWidget);

    connect(mServerListWidget, SIGNAL(addNewServer()), this, SLOT(showAddNewServerWidget()));
    connect(mServerListWidget, SIGNAL(serverClicked(const QModelIndex&)), this, SLOT(connectToHost(const QModelIndex&)));
    connect(mServerListWidget, SIGNAL(editServerClicked(const QModelIndex&)), this, SLOT(editServer(const QModelIndex&)));
    connect(mNewServerWidget, SIGNAL(saveToServerList(const QString&, int, const QString&, const QString&, bool, int, bool, orv_communication_quality_profile_t, const orv_communication_pixel_format_t&)),
            this, SLOT(saveToServerList(const QString&, int, const QString&, const QString&, bool, int, bool, orv_communication_quality_profile_t, const orv_communication_pixel_format_t&)));
    connect(mNewServerWidget, SIGNAL(connectToHost(const QString&, int, const QString&, bool, orv_communication_quality_profile_t, const orv_communication_pixel_format_t&, int)),
            this, SLOT(connectToHost(const QString&, int, const QString&, bool, orv_communication_quality_profile_t, const orv_communication_pixel_format_t&, int)));
    connect(mNewServerWidget, SIGNAL(cancelWidget()), this, SLOT(cancelNewServer()));
    connect(mNewServerWidget, SIGNAL(deleteServerAndCancelWidget(int)), this, SLOT(deleteServerAndCancelNewServer(int)));
    connect(mConnectStateWidget, SIGNAL(abort()), this, SLOT(abortConnect()));
    connect(this, SIGNAL(orvContextChanged(struct orv_context_t*)), mOrvWidget, SLOT(setOrvContext(struct orv_context_t*)));

    mLatencyTestTimer = new QTimer(this);
    mLatencyTestTimer->setInterval(1000);
    connect(mLatencyTestTimer, SIGNAL(timeout()), this, SLOT(latencyTestTick()));
}

TopWidget::~TopWidget()
{
    orv_latency_tester_disconnect(mLatencyTesterClient);
    mLatencyTesterClient = nullptr;
    delete mQtOrvContext;
}

void TopWidget::setInitialHostName(const QString& hostName)
{
    mNewServerWidget->setHostName(hostName);
}

void TopWidget::setInitialPort(uint16_t port)
{
    mNewServerWidget->setPort(port);
}

void TopWidget::setInitialPassword(const QString& password)
{
    mNewServerWidget->setPassword(password);
}

void TopWidget::startConnect()
{
    int internalServerId = -1;
    orv_communication_pixel_format_t customPixelFormat;
    mNewServerWidget->getCustomPixelFormat(&customPixelFormat);
    connectToHost(mNewServerWidget->hostName(), mNewServerWidget->port(), mNewServerWidget->password(), mNewServerWidget->viewOnly(), mNewServerWidget->qualityProfile(), customPixelFormat, internalServerId);
}

void TopWidget::startConnectToLatencyTester()
{
    if (!mQtOrvContext) {
        qCritical() << "Not yet connected to a vnc server. Not trying to connect to latency tester.";
        // TODO: display msgbox?
        return;
    }
    mLatencyTesterData.clear();
    orv_connection_info_t info;
    orv_get_vnc_connection_info(mQtOrvContext->orvContext(), &info, nullptr);
    orv_latency_tester_disconnect(mLatencyTesterClient);
    int port = 7897;
    const char* hostName = info.mHostName;
#if 1 // debugging, usable on my local machine only
    if (hostName == nullptr || strlen(hostName) == 0) {
        hostName = "localhost";
        port = 30007; // local port forward to VM
    }
    else if (strcmp(hostName, "localhost") == 0 && (info.mPort == 30005 || info.mPort == 30006)) {
        port = 30007; // local port forward to VM
    }
#endif
    mLatencyTesterClient = orv_latency_tester_connect(mQtOrvContext->orvContext(), latencyTesterEventCallback, this, hostName, port);
    if (!mLatencyTesterClient) {
        qCritical() << "Failed to connect to" << info.mHostName << "at port" << port;
    }
}

void TopWidget::disconnectFromLatencyTester()
{
    orv_latency_tester_disconnect(mLatencyTesterClient);
    mLatencyTesterClient = nullptr;
}

void TopWidget::saveToServerList(const QString& host, int port, const QString& password, const QString& name, bool savePassword, int internalServerId, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat)
{
    qDebug().nospace() << "Saving connection " << host << ":" << port << " to server list";
    if (internalServerId < 0) {
        internalServerId = mServerListModel->addNewServer(host, port, password, name, savePassword, viewOnly, qualityProfile, customPixelFormat);
        if (internalServerId < 0) {
            QMessageBox::critical(this, tr("Save failed"), tr("Failed to save server to server list"));
        }
        else {
            qDebug() << "Added server with id" << internalServerId;
            mNewServerWidget->setInternalServerId(internalServerId);
        }
    }
    else {
        bool ok = mServerListModel->updateServer(internalServerId, host, port, password, name, savePassword, viewOnly, qualityProfile, customPixelFormat);
        if (!ok) {
            QMessageBox::critical(this, tr("Save failed"), tr("Failed to save server to server list"));
        }
    }
}


void TopWidget::connectToHost(const QModelIndex& index)
{
    QString host = index.data((int)ServerListModel::Roles::HostName).toString();
    int port = index.data((int)ServerListModel::Roles::Port).toInt();
    QString encryptedPassword = index.data((int)ServerListModel::Roles::EncryptedPassword).toString();
    int internalServerId = index.data((int)ServerListModel::Roles::InternalServerId).toInt();
    QString password = mServerListModel->decryptPassword(encryptedPassword);
    bool viewOnly = index.data((int)ServerListModel::Roles::ViewOnly).toBool();
    orv_communication_quality_profile_t qualityProfile = (orv_communication_quality_profile_t)index.data((int)ServerListModel::Roles::CommunicationQualityProfile).toInt();
    orv_communication_pixel_format_t customPixelFormat = index.data((int)ServerListModel::Roles::CustomPixelFormat).value<orv_communication_pixel_format_t>();
    connectToHost(host, port, password, viewOnly, qualityProfile, customPixelFormat, internalServerId);
}

void TopWidget::connectToHost(const QString& host, int port, const QString& password, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat, int internalServerId)
{
    qDebug() << "Connecting to host" << host << "on port" << port;
    mOrvWidget->setDebugBitPlanes(-1);
    mWasDisconnectedWaitForUser = false;
    mLatencyTesterData.clear();
    if (!mQtOrvContext) {
        mQtOrvContext = new OrvContext(this);
        connect(mQtOrvContext, SIGNAL(connectResult(const orv_connect_result_t*)), this, SLOT(handleConnectEvent(const orv_connect_result_t*)));
        connect(mQtOrvContext, SIGNAL(disconnected(const orv_disconnected_t*)), this, SLOT(handleDisconnectedEvent(const orv_disconnected_t*)));
        connect(mQtOrvContext, SIGNAL(cutText(const QString&)), this, SLOT(handleCutTextEvent(const QString&)));
        connect(mQtOrvContext, SIGNAL(framebufferUpdated(const orv_event_framebuffer_t*)), this, SLOT(handleFramebufferUpdatedEvent(const orv_event_framebuffer_t*)));
        connect(mQtOrvContext, SIGNAL(framebufferUpdateRequestFinished()), this, SLOT(handleFramebufferUpdateRequestFinishedEvent()));
        connect(mQtOrvContext, SIGNAL(cursorUpdated()), this, SLOT(handleCursorUpdatedEvent()));
        connect(mQtOrvContext, SIGNAL(bell()), this, SLOT(handleBellEvent()));
        if (!mQtOrvContext->orvContext()) {
            QMessageBox::critical(this, tr("Unable to connect"), tr("Unable to create a new OpenRV context, cannot connect to a new host."));
            delete mQtOrvContext;
            mQtOrvContext = nullptr;
            return;
        }
        emit orvContextChanged(mQtOrvContext->orvContext());
    }
    mConnectStateWidget->clear();
    mConnectStateWidget->setHost(host, port);
    orv_error_t error;
    QByteArray hostLocal8bit = host.toLocal8Bit();
    QByteArray passwordLocal8bit = password.toLocal8Bit(); // NOTE: Must be (and is) NUL-terminated
    orv_connect_options_t connectOptions;
    orv_connect_options_default(&connectOptions);
    orv_set_user_data_int(mQtOrvContext->orvContext(), ORV_USER_DATA_1, internalServerId);
    connectOptions.mCommunicationQualityProfile = qualityProfile;
    orv_communication_pixel_format_copy(&connectOptions.mCommunicationPixelFormat, &customPixelFormat); // only used if quality profile is custom
    connectOptions.mViewOnly = viewOnly;
    if (orv_connect(mQtOrvContext->orvContext(), hostLocal8bit.constData(), (uint16_t)port, passwordLocal8bit.constData(), &connectOptions, &error) != 0) {
        mConnectStateWidget->setError(QString::fromLatin1(error.mErrorMessage));
    }
    changeCurrentWidgetTo(WidgetType::ConnectStateOrResult);
}

void TopWidget::editServer(const QModelIndex& index)
{
    changeCurrentWidgetTo(WidgetType::NewServer);
    mNewServerWidget->setInternalServerId(index.data((int)ServerListModel::Roles::InternalServerId).toInt());
    mNewServerWidget->setHostName(index.data((int)ServerListModel::Roles::HostName).toString());
    mNewServerWidget->setName(index.data((int)ServerListModel::Roles::Name).toString());
    mNewServerWidget->setPassword(mServerListModel->decryptPassword(index.data((int)ServerListModel::Roles::EncryptedPassword).toString()));
    mNewServerWidget->setPort(index.data((int)ServerListModel::Roles::Port).toInt());
    mNewServerWidget->setConnectImmediately(false);
    mNewServerWidget->setSaveToServerList(true);
    mNewServerWidget->setSavePassword(index.data((int)ServerListModel::Roles::SavePassword).toBool());
    mNewServerWidget->setViewOnly(index.data((int)ServerListModel::Roles::ViewOnly).toBool());
    mNewServerWidget->setQualityProfile((orv_communication_quality_profile_t)index.data((int)ServerListModel::Roles::CommunicationQualityProfile).toInt());
    mNewServerWidget->setCustomPixelFormat(index.data((int)ServerListModel::Roles::CustomPixelFormat).value<orv_communication_pixel_format_t>());
}

void TopWidget::cancelNewServer()
{
    changeCurrentWidgetTo(WidgetType::ServerList);
}

void TopWidget::deleteServerAndCancelNewServer(int internalServerId)
{
    mServerListModel->deleteServer(internalServerId);
    changeCurrentWidgetTo(WidgetType::ServerList);
}

void TopWidget::abortConnect()
{
    if (!mQtOrvContext || mWasDisconnectedWaitForUser) {
        changeCurrentWidgetTo(WidgetType::ServerList);
        return;
    }
    orv_disconnect(mQtOrvContext->orvContext());
}

void TopWidget::handleConnectEvent(const orv_connect_result_t* data)
{
    if (mLayout->currentWidget() != mConnectStateWidget) {
        // should never happen
        return;
    }
    if (data->mError.mHasError) {
        mConnectStateWidget->setError(tr("Error code: %1, sub-code: %2\nError message: %3").arg(data->mError.mErrorCode).arg(data->mError.mSubErrorCode).arg(data->mError.mErrorMessage));
        return;
    }
    int internalServerId = orv_get_user_data_int(mQtOrvContext->orvContext(), ORV_USER_DATA_1);
    if (internalServerId > 0) {
        mServerListModel->updateLastConnectoinForServer(internalServerId, QDateTime::currentDateTime());
    }
    QString desktopName = QString::fromLatin1(data->mDesktopName); // TODO: is latin-1 correct? what encoding does desktopname use?
    mOrvWidget->setFramebufferSize(data->mFramebufferWidth, data->mFramebufferHeight, desktopName);
    mLayout->setCurrentWidget(mOrvWidget);
#if 0
    const uint16_t x = 0;
    const uint16_t y = 0;
    orv_request_framebuffer_update(mQtOrvContext->orvContext(), x, y, mOrvWidget->framebufferWidth(), mOrvWidget->framebufferHeight());
#else
    orv_request_framebuffer_update_full(mQtOrvContext->orvContext());
#endif
}

void TopWidget::handleDisconnectedEvent(const orv_disconnected_t* data)
{
    orv_latency_tester_disconnect(mLatencyTesterClient);
    mLatencyTesterClient = nullptr;
    mLatencyTesterData.clear();
    if (!mQtOrvContext) {
        return;
    }
    // TODO: display "disconnected" widget (with a "dismiss" button that returns to the
    //       mNewServerWidget)
    //       UPDATE: maybe display an overlay over the OpenRV widget instead!
    //       -> otherwise the screen goes suddenly blank and the user may lose information (= screen
    //          contents)

    changeCurrentWidgetTo(WidgetType::ConnectStateOrResult);
    if (!data->mError.mHasError) {
        mConnectStateWidget->setError(tr("The connection was closed (no error)."));
    }
    else {
        mConnectStateWidget->setError(tr("The connection was closed.\nError code: %1, sub-code: %2\nError message: %3").arg(data->mError.mErrorCode).arg(data->mError.mSubErrorCode).arg(data->mError.mErrorMessage));
    }
    // keep displaying this the connection result widget until user confirmed the message
    mWasDisconnectedWaitForUser = true;

    if (data->mError.mHasError) {
        qDebug("Disconnected due to error: %s", data->mError.mErrorMessage);
    }
}

void TopWidget::handleBellEvent()
{
    qDebug("Received bell event");
    // TODO: ring a bell!
    //   -> probably display a passive popup
}

void TopWidget::handleCutTextEvent(const QString& text)
{
    // TODO: save in clipboard
}

void TopWidget::handleFramebufferUpdatedEvent(const orv_event_framebuffer_t* data)
{
    if (!mQtOrvContext) {
        return;
    }
#if defined(Q_OS_UNIX)
    struct timeval eventReceiveTimestamp;
    gettimeofday(&eventReceiveTimestamp, nullptr);
#endif
    const orv_framebuffer_t* framebuffer = orv_acquire_framebuffer(mQtOrvContext->orvContext());
    if (framebuffer) {
        mOrvWidget->updateFramebuffer(framebuffer, data->mX, data->mY, data->mWidth, data->mHeight);
    }
    else {
        // TODO: handle error...
    }
    orv_release_framebuffer(mQtOrvContext->orvContext());

#if defined(Q_OS_UNIX)
    struct timeval updatedTimestamp;
    gettimeofday(&updatedTimestamp, nullptr);
    qint64 eventReceiveUs = eventReceiveTimestamp.tv_sec * 1000 * 1000 + eventReceiveTimestamp.tv_usec;
    qint64 updatedUs = updatedTimestamp.tv_sec * 1000 * 1000 + updatedTimestamp.tv_usec;
    handleLatencyTesterFramebufferUpdate(data, eventReceiveUs, updatedUs);
#else
    // NOTE: latency tester client currently not supported on windows clients.
    //       if we ever want to support it, we would have to use the same gettimeofday() replacement
    //       here as we use in the latency tester client, to ensure the timestamps are comparable.
#endif
}

void TopWidget::handleFramebufferUpdateRequestFinishedEvent()
{
    // TODO: make configurable!
    // -> make extremely low for server latency tests!
    //const int updateDelay = 200; // do not update immediately to reduce network usage
    const int updateDelay = 0;
    QTimer::singleShot(updateDelay, this, SLOT(requestFramebufferUpdate()));
}

void TopWidget::handleCursorUpdatedEvent()
{
    if (!mQtOrvContext) {
        return;
    }
    const orv_cursor_t* cursor = orv_acquire_cursor(mQtOrvContext->orvContext());
    if (cursor) {
        mOrvWidget->updateCursor(cursor);
    }
    orv_release_cursor(mQtOrvContext->orvContext());
}

void TopWidget::changeCurrentWidgetTo(WidgetType w)
{
    bool canAbortConnection = true;
    switch (w) {
        case WidgetType::ServerList:
            mWasDisconnectedWaitForUser = false;
            canAbortConnection = false;
            mLayout->setCurrentWidget(mServerListWidget);
            break;
        case WidgetType::NewServer:
            mWasDisconnectedWaitForUser = false;
            canAbortConnection = false;
            mNewServerWidget->reset();
            mLayout->setCurrentWidget(mNewServerWidget);
            break;
        case WidgetType::ConnectStateOrResult:
            canAbortConnection = true;
            mLayout->setCurrentWidget(mConnectStateWidget);
            break;
        case WidgetType::OpenRV:
            canAbortConnection = true;
            mLayout->setCurrentWidget(mOrvWidget);
            break;
    }
    emit canAbortConnectionUpdated(canAbortConnection);
}

void TopWidget::showAddNewServerWidget()
{
    if (mQtOrvContext && orv_is_connected(mQtOrvContext->orvContext())) {
        qCritical() << "ERROR: Still connected to a server, connection must be aborted first";
        return;
    }
    changeCurrentWidgetTo(WidgetType::NewServer);
}

void TopWidget::showServerListWidget()
{
    if (mQtOrvContext && orv_is_connected(mQtOrvContext->orvContext())) {
        qCritical() << "ERROR: Still connected to a server, connection must be aborted first";
        return;
    }
    changeCurrentWidgetTo(WidgetType::ServerList);
}

void TopWidget::showConnectionInfo()
{
    if (mConnectionInfoDialog) {
        mConnectionInfoDialog->show();
        return;
    }
    mConnectionInfoDialog = new QDialog(this);
    mConnectionInfoDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(mConnectionInfoDialog, SIGNAL(destroyed()), this, SLOT(connectionInfoDestroyed()));
    QVBoxLayout* l = new QVBoxLayout(mConnectionInfoDialog);
    ScrollableConnectionInfoWidget* info = new ScrollableConnectionInfoWidget(mConnectionInfoDialog);
    l->setContentsMargins(QMargins());
    l->addWidget(info);
    info->setOrvContext(mQtOrvContext->orvContext());
    connect(this, SIGNAL(orvContextChanged(struct orv_context_t*)), info, SLOT(setOrvContext(struct orv_context_t*)));
    mConnectionInfoDialog->show();
}

void TopWidget::connectionInfoDestroyed()
{
    mConnectionInfoDialog = nullptr;
}

/**
 * Send a framebuffer update request to the server.
 *
 * This message needs to be sent regularly (whenever a response to the previous message has been
 * finished), otherwise we receive no more updates
 **/
void TopWidget::requestFramebufferUpdate()
{
    if (mQtOrvContext) {
        orv_request_framebuffer_update_full(mQtOrvContext->orvContext());
    }
}

void latencyTesterEventCallback(struct orv_latency_tester_client_t* client, orv_latency_tester_event_type_t event, const void* eventData, void* userData)
{
    // NOTE: is called on a different thread!
    TopWidget* topWidget = (TopWidget*)userData;
    switch (event) {
        case ORV_LATENCY_TESTER_CONNECT_RESULT:
        {
            const orv_connect_result_t* connectResult = (const orv_connect_result_t*)eventData;
            if (connectResult) {
                qDebug() << topWidget;
                QMetaObject::invokeMethod(topWidget, "canAbortConnectionToLatencyTesterUpdated", Qt::QueuedConnection, Q_ARG(bool, !connectResult->mError.mHasError));
                if (connectResult->mError.mHasError) {
                    qDebug() << "Failed to connect to latency tester, error:" << connectResult->mError.mErrorMessage;
                }
                else {
                    qDebug() << "Connected to latency tester at" << connectResult->mHostName;
                }
            }
            break;
        }
        case ORV_LATENCY_TESTER_DISCONNECTED:
        {
            const orv_disconnected_t* disconnected = (const orv_disconnected_t*)eventData;
            if (disconnected) {
                qDebug() << "disconnected from" << disconnected->mHostName;
                QMetaObject::invokeMethod(topWidget, "canAbortConnectionToLatencyTesterUpdated", Qt::QueuedConnection, Q_ARG(bool, false));
            }
            break;
        }
        case ORV_LATENCY_TESTER_UPDATE_RESPONSE:
        {

            const orv_latency_tester_update_response_t* response = (const orv_latency_tester_update_response_t*)eventData;
            if (eventData) {
                QRect previousRect = QRect(QPoint(response->mPreviousTopLeftX, response->mPreviousTopLeftY), QPoint(response->mPreviousBottomRightX, response->mPreviousBottomRightY));
                QRect newRect = QRect(QPoint(response->mNewTopLeftX, response->mNewTopLeftY), QPoint(response->mNewBottomRightX, response->mNewBottomRightY));
                qint64 clientSendTimeUs = response->mClientSendTimestampSec * 1000 * 1000 + response->mClientSendTimestampUSec;
                qint64 clientReceiveTimeUs = response->mClientReceiveTimestampSec * 1000 * 1000 + response->mClientReceiveTimestampUSec;
                QMetaObject::invokeMethod(topWidget, "addLatencyTesterData", Qt::QueuedConnection, Q_ARG(const QRect&, previousRect), Q_ARG(const QRect&, newRect), Q_ARG(qint64, clientSendTimeUs), Q_ARG(qint64, clientReceiveTimeUs));
            }
            break;
        }
        default:
            qWarning() << "WARNING: Unhandled latency tester event type" << (int)event;
            return;
    }
}

void TopWidget::sendCanAbortConnectionToLatencyTesterUpdated(bool canAbortConnection)
{
    if (!canAbortConnection) {
        mLatencyTesterData.clear();
    }
    emit canAbortConnectionToLatencyTesterUpdated(canAbortConnection);
}

void TopWidget::startLatencyTest()
{
    mLatencyTestTimer->start();
}

void TopWidget::stopLatencyTest()
{
    mLatencyTestTimer->stop();
}

/**
 * @pre Called by the main thread
 **/
void TopWidget::latencyTestTick()
{
    mLatencyTesterData.clear(); // clear old data, in case we did never receive a proper response in time (to avoid filling the list with obsolete data in case race condition occurs where the update is processed before the latency data arrives)
    if (orv_latency_tester_request_update(mLatencyTesterClient) != 0) {
        qDebug() << "Failed to send update request to latency tester";
    }
}

void TopWidget::handleLatencyTesterFramebufferUpdate(const orv_event_framebuffer_t* data, qint64 eventReceiveUs, qint64 updatedUs)
{
    if (!mLatencyTesterData.isEmpty()) {
        // TODO (low priority): implement proper regions:
        //      the update is only fully received once we have received updates for ALL parts of the
        //      rects (previous+new rect).
        //      if this update contains only a few pixels of these rects, we would have to wait for
        //      more data, possibly maintaining a bitmap for all changes.
        //      however for simplicity we currently only care for the FIRST update on these areas
        //      and assume the update contains all data.
        //      this should normally be true in most cases (but give too optimistic results in the
        //      border cases) - this is good enough for rough estimates.
        QRect rect(QPoint(data->mX, data->mY), QSize(data->mWidth, data->mHeight));
        int i = 0;
        while (i < mLatencyTesterData.count()) {
            const LatencyTesterData& latencyData = mLatencyTesterData.at(i);
            if (latencyData.mPreviousRect.intersects(rect) || latencyData.mNewRect.intersects(rect)) {
                handleLatencyData(latencyData, eventReceiveUs, updatedUs);
                mLatencyTesterData.removeAt(i);
            }
            else {
                i++;
            }
        }
        // FIXME: race condition:
        //   it is possible (but unlikely) for an OpenRV event to arrive BEFORE the latency tester
        //   data is received. we should somehow handle this properly.
        //   however it might be sufficient to simply clear all latency data whenever a new update
        //   request is sent (which we currently do) and ignore all data if the race condition
        //   occurs (i.e. data is lost).
        //   -> this is "good enough" because this is a development feature only anyway.
    }
}

/**
 * @pre Called by the main thread
 **/
void TopWidget::addLatencyTesterData(const QRect& previousRect, const QRect& newRect, qint64 clientSendTimeUs, qint64 clientReceiveTimeUs)
{
    LatencyTesterData data;
    data.mPreviousRect = previousRect;
    data.mNewRect = newRect;
    data.mClientSendTimeUs = clientSendTimeUs;
    data.mClientReceiveTimeUs = clientReceiveTimeUs;
    mLatencyTesterData.append(data);
}

/**
 * Called when the OpenRV data for the @p latencyData is received, i.e. the timestamps can be
 * processed now.
 **/
void TopWidget::handleLatencyData(const LatencyTesterData& latencyData, qint64 orvEventReceiveUs, qint64 orvUpdatedUs)
{
    qint64 sendReceiveDurationUs = latencyData.mClientReceiveTimeUs - latencyData.mClientSendTimeUs; // time spent from sending update request to receiving confirmation that update was performed at the server. this is the theoretical minimum time that the OpenRV data could be received in (assuming OpenRV also uses tcp).
    qint64 fullDurationUs = orvUpdatedUs - latencyData.mClientSendTimeUs; // time from sending update request until local framebuffer was updated with new data
    qint64 orvUpdatedDuration = orvUpdatedUs - orvEventReceiveUs; // time spent from receiving event to updating local framebuffer
    //qDebug() << "Fully processed latency tester data, clientSendTimeUs:" << latencyData.mClientSendTimeUs << "receiveUs:" << latencyData.mClientReceiveTimeUs << "sendReceiveDurationUs:" << sendReceiveDurationUs << "orvEventTimeUs:" << orvEventReceiveUs << "orvUpdatedUs:" << orvUpdatedUs << "orvUpdatedDuration:" << orvUpdatedDuration << "fullDurationUs:" << fullDurationUs;
    qDebug() << "Fully processed latency tester data, sendReceiveDurationUs:" << sendReceiveDurationUs << "orvUpdatedDuration:" << orvUpdatedDuration << "fullDurationUs:" << fullDurationUs;
}

void TopWidget::setDebugBitPlanes(int planes)
{
    mOrvWidget->setDebugBitPlanes(planes);
}

