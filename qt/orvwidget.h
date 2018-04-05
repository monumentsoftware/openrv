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

#ifndef ORVWIDGET_H
#define ORVWIDGET_H

#include <QtWidgets/QAbstractScrollArea>
#include <QtCore/QHash>

class QTimer;

class FramebufferWidget;

struct orv_framebuffer_t;

class OrvWidget : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit OrvWidget(QWidget* widget);
    virtual ~OrvWidget();

    void setFramebufferSize(uint16_t width, uint16_t height, const QString& desktopName);
    void updateFramebuffer(const struct orv_framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void updateCursor(const struct orv_cursor_t* cursor);
    void setScaleFramebuffer(bool scale);
    bool scaleFramebuffer() const;
    void setDebugBitPlanes(int planes);

    uint16_t framebufferWidth() const;
    uint16_t framebufferHeight() const;

    QPoint mapToFramebuffer(const QPoint& p) const;

public slots:
    void setOrvContext(struct orv_context_t* ctx);

protected slots:
    void framebufferWidgetInitializedOpenGL();

protected:
    enum ScrollFlag
    {
        SCROLL_NONE = 0x00,
        SCROLL_LEFT = 0x01,
        SCROLL_UP = 0x02,
        SCROLL_RIGHT = 0x04,
        SCROLL_DOWN = 0x08,
    };
    Q_DECLARE_FLAGS(ScrollFlags, ScrollFlag);
protected:
    virtual void mousePressEvent(QMouseEvent* e) override;
    virtual void mouseReleaseEvent(QMouseEvent* e) override;
    virtual void mouseMoveEvent(QMouseEvent* e) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* e) override;
    virtual void wheelEvent(QWheelEvent* e) override;
    virtual void keyPressEvent(QKeyEvent* e) override;
    virtual void keyReleaseEvent(QKeyEvent* e) override;
    virtual void resizeEvent(QResizeEvent* e) override;
    virtual void scrollContentsBy(int dx, int dy) override;
    ScrollFlags calculateScrollFlags(const QPoint& pos) const;
    static uint8_t calculateButtonMask(const QMouseEvent* e);
    static uint8_t calculateButtonMask(const QWheelEvent* e);
    static uint8_t calculateButtonMask(const Qt::MouseButtons buttonMask);
    void updateScrollBars();

protected slots:
    void scrollTimeout();

private:
    struct orv_context_t* mOrvContext = nullptr;
    FramebufferWidget* mFramebufferWidget = nullptr;
    QString mDesktopName;
    QTimer* mScrollTimer = nullptr;
    int mDebugBitPlanes = -1;
    QHash<int, int> mSentKeysmsForKeyPress;
};

#endif

