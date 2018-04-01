#include "orv_context_qt.h"

#include <QEvent>
#include <QCoreApplication>
#include <QtDebug>

int OrvContext::mQtEventType = -1;

class OrvEvent : public QEvent
{
public:
    OrvEvent(int qtType, orv_event_t* event) : QEvent((QEvent::Type)qtType), mOrvEvent(event)
    {
    }
    ~OrvEvent()
    {
        orv_event_destroy(mOrvEvent);
    }
    orv_event_t* mOrvEvent;
};

static void orv_qt_callback(orv_context_t* context, orv_event_t* event);


OrvContext::OrvContext(QObject* parent)
    : QObject(parent)
{
    orv_config_t config;
    orv_config_default(&config);
    //config.mLogCallback = logOrvMessage;
    config.mEventCallback = orv_qt_callback;
    mOrvContext = orv_init(&config);
    orv_set_user_data(mOrvContext, ORV_USER_DATA_0, this);
}

OrvContext::~OrvContext()
{
    orv_set_user_data(mOrvContext, ORV_USER_DATA_0, nullptr);
    orv_destroy(mOrvContext);
}

/**
 * Optional init function for the internal Qt event type, in case the application uses hardcoded
 * custom event types that do not use @ref QEvent::registerEventType().
 *
 * This function registers a new qt event using @ref QEvent::registerEventType() with the specified
 * hint.
 *
 * This function is normally not necessary, the event type is registered automatically when needed.
 **/
void OrvContext::initQtEventType(int typeHint)
{
    mQtEventType = QEvent::registerEventType(typeHint);
}

int OrvContext::qtEventType()
{
    if (mQtEventType < 0) {
        initQtEventType(-1);
    }
    return mQtEventType;
}

/**
 * @return The OpenRV context maintained by this object, as created by @ref orv_init. The context
 *         has this object in the @ref ORV_USER_DATA_0.
 **/
orv_context_t* OrvContext::orvContext() const
{
    return mOrvContext;
}

/**
 * Reimplemented to handle the OpenRV events.
 *
 * OpenRV events are received by the internal callback on the OpenRV thread and sent to the thread
 * that owns this object, which calls this function.
 *
 * This function calls the specialized event handling functions of this object.
 **/
bool OrvContext::event(QEvent* e)
{
    if (e->type() != mQtEventType) {
        return QObject::event(e);
    }
    OrvEvent* orvEvent = static_cast<OrvEvent*>(e);
    orv_event_type_t orvEventType = ORV_EVENT_NONE;
    if (orvEvent->mOrvEvent) {
        orvEventType = orvEvent->mOrvEvent->mEventType;
    }
    switch (orvEvent->mOrvEvent->mEventType) {
        case ORV_EVENT_NONE:
        case ORV_EVENT_THREAD_STARTED:
        case ORV_EVENT_THREAD_ABOUT_TO_STOP:
            // should never be reached (not sent to this object)
            break;
        case ORV_EVENT_CONNECT_RESULT:
        {
            orv_connect_result_t* data = (orv_connect_result_t*)orvEvent->mOrvEvent->mEventData;
            handleConnectResultEvent(data);
            break;
        }
        case ORV_EVENT_BELL:
            handleBellEvent();
            break;
        case ORV_EVENT_DISCONNECTED:
        {
            const orv_disconnected_t* data = (orv_disconnected_t*)orvEvent->mOrvEvent->mEventData;
            handleDisconnectedEvent(data);
            break;
        }
        case ORV_EVENT_CUT_TEXT:
        {
            QString text = QString::fromLatin1(((orv_cut_text_t*)orvEvent->mOrvEvent->mEventData)->mText);
            handleCutTextEvent(text);
            break;
        }
        case ORV_EVENT_FRAMEBUFFER_UPDATED:
        {
            const orv_event_framebuffer_t* data = (orv_event_framebuffer_t*)orvEvent->mOrvEvent->mEventData;
            handleFramebufferUpdatedEvent(data);
            break;
        }
        case ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED:
            handleFramebufferUpdateRequestFinishedEvent();
            break;
        case ORV_EVENT_CURSOR_UPDATED:
            handleCursorUpdatedEvent();
            break;
    }
    return true;
}

/**
 * Called for @ref ORV_EVENT_CONNECT_RESULT events. The default implementation emits @ref
 * connectResult.
 **/
void OrvContext::handleConnectResultEvent(const orv_connect_result_t* data)
{
    emit connectResult(data);
}

/**
 * Called for @ref ORV_EVENT_BELL events. The default implementation emits @ref
 * bell.
 **/
void OrvContext::handleBellEvent()
{
    emit bell();
}

/**
 * Called for @ref ORV_EVENT_DISCONNECTED events. The default implementation emits @ref
 * disconnected.
 **/
void OrvContext::handleDisconnectedEvent(const orv_disconnected_t* data)
{
    emit disconnected(data);
}

/**
 * Called for @ref ORV_EVENT_CUT_TEXT events. The default implementation emits @ref
 * cutText.
 **/
void OrvContext::handleCutTextEvent(const QString& text)
{
    emit cutText(text);
}

/**
 * Called for @ref ORV_EVENT_FRAMEBUFFER_UPDATED events. The default implementation emits @ref
 * framebufferUpdated.
 **/
void OrvContext::handleFramebufferUpdatedEvent(const orv_event_framebuffer_t* data)
{
    emit framebufferUpdated(data);
}

/**
 * Called for @ref ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED events. The default implementation emits @ref
 * framebufferUpdateRequestFinished.
 **/
void OrvContext::handleFramebufferUpdateRequestFinishedEvent()
{
    emit framebufferUpdateRequestFinished();
}

/**
 * Called for @ref ORV_EVENT_CURSOR_UPDATED events. The default implementation emits @ref
 * cursorUpdated.
 **/
void OrvContext::handleCursorUpdatedEvent()
{
    emit cursorUpdated();
}

void orv_qt_callback(orv_context_t* context, orv_event_t* event)
{
    if (!event) {
        return;
    }
    if (!context) {
        orv_event_destroy(event);
        return;
    }
    OrvContext* qtContext = (OrvContext*)orv_get_user_data(context, ORV_USER_DATA_0);
    if (!qtContext) {
        orv_event_destroy(event);
        return;
    }
    if (context != qtContext->orvContext()) {
        orv_event_destroy(event);
        return;
    }
    bool ownershipTransferred = false;
    switch (event->mEventType) {
        case ORV_EVENT_NONE:
        case ORV_EVENT_THREAD_STARTED:
        case ORV_EVENT_THREAD_ABOUT_TO_STOP:
            break;
        case ORV_EVENT_CONNECT_RESULT:
        case ORV_EVENT_DISCONNECTED:
        case ORV_EVENT_CUT_TEXT:
        case ORV_EVENT_FRAMEBUFFER_UPDATED:
        case ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED:
        case ORV_EVENT_CURSOR_UPDATED:
        case ORV_EVENT_BELL:
        {
            QCoreApplication::instance()->postEvent(qtContext, new OrvEvent(OrvContext::qtEventType(), event));
            ownershipTransferred = true;
            break;
        }
    }
    if (!ownershipTransferred) {
        orv_event_destroy(event);
    }
}

