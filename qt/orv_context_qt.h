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

#ifndef ORV_CONTEXT_QT_H
#define ORV_CONTEXT_QT_H

#include <QObject>
#include <libopenrv/libopenrv.h>

/**
 * Qt based wrapper around orv_context_t and @ref orv_init()/@ref orv_destroy().
 *
 * This object provides a OpenRV context with a callback that automatically sends events to this
 * object. It must be created from within a thread with a Qt event loop (usually simply the main
 * thread).
 **/
class OrvContext : public QObject
{
    Q_OBJECT
public:
    OrvContext(QObject* parent = nullptr);
    virtual ~OrvContext();
    OrvContext(const OrvContext&) = delete;
    OrvContext& operator=(const OrvContext&) = delete;

    static void initQtEventType(int typeHint);

    orv_context_t* orvContext() const;
    static int qtEventType();

signals:
    void connectResult(const orv_connect_result_t* result);
    void disconnected(const orv_disconnected_t* data);
    void cutText(const QString& text);
    void framebufferUpdated(const orv_event_framebuffer_t* data);
    void framebufferUpdateRequestFinished();
    void cursorUpdated();
    void bell();

protected:
    virtual void handleConnectResultEvent(const orv_connect_result_t* data);
    virtual void handleBellEvent();
    virtual void handleDisconnectedEvent(const orv_disconnected_t* data);
    virtual void handleCutTextEvent(const QString& text);
    virtual void handleFramebufferUpdatedEvent(const orv_event_framebuffer_t* data);
    virtual void handleFramebufferUpdateRequestFinishedEvent();
    virtual void handleCursorUpdatedEvent();

    virtual bool event(QEvent* e) override;

private:
    static int mQtEventType;
    orv_context_t* mOrvContext = nullptr;
};

#endif

