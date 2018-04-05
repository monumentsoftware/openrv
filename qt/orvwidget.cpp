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

#include "orvwidget.h"

#include "framebufferwidget.h"

#include <libopenrv/libopenrv.h>
#include <libopenrv/orv_qtkey_to_xkeysym.h>

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QScrollBar>
#include <QtGui/QMouseEvent>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtCore/QtDebug>

#include <algorithm>

#define SCROLL_INTERVAL 100
#define SCROLL_STEP 20

OrvWidget::OrvWidget(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    mFramebufferWidget = new FramebufferWidget(viewport());
    viewport()->setMouseTracking(true);
    mFramebufferWidget->setMouseTracking(true);

    mScrollTimer = new QTimer(this);
    mScrollTimer->setInterval(SCROLL_INTERVAL);
    connect(mScrollTimer, SIGNAL(timeout()), this, SLOT(scrollTimeout()));
}

OrvWidget::~OrvWidget()
{
}

void OrvWidget::setScaleFramebuffer(bool scale)
{
    mFramebufferWidget->setScaleFramebuffer(scale);
    updateScrollBars();
}

bool OrvWidget::scaleFramebuffer() const
{
    return mFramebufferWidget->scaleFramebuffer();
}

void OrvWidget::setOrvContext(orv_context_t* ctx)
{
    mOrvContext = ctx;
}

void OrvWidget::setFramebufferSize(uint16_t width, uint16_t height, const QString& desktopName)
{
    mDesktopName = desktopName;
    mFramebufferWidget->resetFramebuffer(width, height);
    // NOTE: updateFramebuffer() is required, otherwise framebuffer remains empty.
    updateScrollBars();
}

void OrvWidget::updateFramebuffer(const struct orv_framebuffer_t* framebuffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    mFramebufferWidget->updateFramebuffer(framebuffer, x, y, width, height);
}

void OrvWidget::updateCursor(const struct orv_cursor_t* cursor)
{
    mFramebufferWidget->updateCursor(cursor);

    if (cursor && cursor->mIsValid) {
        // once the cursor was received once, VNC will always handle the cursor (server is known to
        // support the Cursor extension). hide the local cursor.
        viewport()->setCursor(Qt::BlankCursor);
    }
    else {
        // an invalid cursor  was received. may happen if server sent an empty rect for some reason!
        viewport()->setCursor(QCursor());
    }
}

uint16_t OrvWidget::framebufferWidth() const
{
    return mFramebufferWidget->framebufferWidth();
}

uint16_t OrvWidget::framebufferHeight() const
{
    return mFramebufferWidget->framebufferHeight();
}

void OrvWidget::resizeEvent(QResizeEvent* e)
{
    QAbstractScrollArea::resizeEvent(e);
    mFramebufferWidget->resize(e->size());
    updateScrollBars();
}

void OrvWidget::mousePressEvent(QMouseEvent* e)
{
    if (mOrvContext) {
        QPoint p = mapToFramebuffer(e->pos());
        uint8_t buttonMask = calculateButtonMask(e);
        orv_send_pointer_event(mOrvContext, p.x(), p.y(), buttonMask);
    }
}

void OrvWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (mOrvContext) {
        QPoint p = mapToFramebuffer(e->pos());
        uint8_t buttonMask = calculateButtonMask(e);
        orv_send_pointer_event(mOrvContext, p.x(), p.y(), buttonMask);
    }
}

void OrvWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (mOrvContext) {
        QPoint p = mapToFramebuffer(e->pos());
        uint8_t buttonMask = calculateButtonMask(e);
        orv_send_pointer_event(mOrvContext, p.x(), p.y(), buttonMask);
    }
}

void OrvWidget::mouseMoveEvent(QMouseEvent* e)
{
    ScrollFlags flags = calculateScrollFlags(e->pos());
    if (flags == SCROLL_NONE && mScrollTimer->isActive()) {
        mScrollTimer->stop();
    }
    else if (flags != SCROLL_NONE && !mScrollTimer->isActive()) {
        mScrollTimer->start();
    }

    if (mOrvContext) {
        QPoint p = mapToFramebuffer(e->pos());
        uint8_t buttonMask = calculateButtonMask(e);
        orv_send_pointer_event(mOrvContext, p.x(), p.y(), buttonMask);

        // schedule a paint event - cursor is normally painted by OpenGL, we normally do not use the
        // normal cursor.
        mFramebufferWidget->update();
    }
}

void OrvWidget::wheelEvent(QWheelEvent* e)
{
    if (!mOrvContext) {
        return;
    }
    const QPoint angleDelta = e->angleDelta();
    if (angleDelta.isNull()) {
        return;
    }
    QPoint p = mapToFramebuffer(e->pos());
    uint8_t buttonMask = calculateButtonMask(e);
    uint8_t origButtonMask = buttonMask;
    if (angleDelta.x() < 0) {
        buttonMask |= ORV_MOUSE_WHEEL_LEFT;
    }
    else if (angleDelta.x() > 0) {
        buttonMask |= ORV_MOUSE_WHEEL_RIGHT;
    }
    if (angleDelta.y() < 0) {
        buttonMask |= ORV_MOUSE_WHEEL_DOWN;
    }
    else if (angleDelta.y() > 0) {
        buttonMask |= ORV_MOUSE_WHEEL_UP;
    }
    orv_send_pointer_event(mOrvContext, p.x(), p.y(), buttonMask);
    orv_send_pointer_event(mOrvContext, p.x(), p.y(), origButtonMask);
}

void OrvWidget::keyPressEvent(QKeyEvent* e)
{
    if (!mOrvContext) {
        return;
    }
    const uint8_t isDown = 1;
    int keysym = orv_qt_key_to_keysym((int)e->key(), (int)e->modifiers(), e->text());
    if (keysym > 0) {
        mSentKeysmsForKeyPress.insert(e->key(), keysym);
        orv_send_key_event(mOrvContext, isDown, keysym);
    }
}

void OrvWidget::keyReleaseEvent(QKeyEvent* e)
{
    if (!mOrvContext) {
        return;
    }
    // NOTE: We want to send exactly the same keysym on release as we sent on press, so we normally
    //       do not perform the mapping of e->key() to keycode, but rather track the keysym on press
    //       event and repeat it here.
    //       (Note: The same key() may map to multiple keysyms, to the mapping is not always unique.
    //       A simple example is uppercase/lowercase, which has the same Qt::Key and also the same
    //       keycode on the keyboard, but multiple keysyms depending on the modifier keys pressed.
    //       Even when ignoring the Shift modifier (as we currently do), the same applies to other
    //       keys as well)
    int keysym = 0;
    if (mSentKeysmsForKeyPress.contains(e->key())) {
        keysym = mSentKeysmsForKeyPress.value(e->key());
        mSentKeysmsForKeyPress.remove(e->key());
    }
    else {
        keysym = orv_qt_key_to_keysym((int)e->key(), (int)e->modifiers(), e->text());
    }
    const uint8_t isDown = 0;
    if (keysym > 0) {
        mSentKeysmsForKeyPress.insert(e->key(), keysym);
        orv_send_key_event(mOrvContext, isDown, keysym);
    }
#if defined(Q_OS_OSX)
    // workaround for https://bugreports.qt.io/browse/QTBUG-36839
    // when the command key (== Qt::Key_Control in qt on osx) is pressed, key release events for
    // other keys are NOT received by QWidgets anymore.
    // as a (crude) workaround, we force a key release event to be sent for all currently
    // pressed keys, when the command key is released.
    //
    // this is very sub-optimal:
    // 1) release events are not sent until the command key is pressed as well
    // 2) if the command key is released before the other key(s) are released, those release
    //    events are incorrect.
    // nevertheless, at least it prevents us from missing release events completely.
    if (e->key() == Qt::Key_Control) {
        qDebug() << "osx command key released, sending fake release events for all other pressed keys.";
        foreach (int k, mSentKeysmsForKeyPress.keys()) {
            keysym = mSentKeysmsForKeyPress.value(k);
            qDebug() << "sending fake release event for keysym" << keysym;
            orv_send_key_event(mOrvContext, isDown, keysym);
        }
        mSentKeysmsForKeyPress.clear();
    }
#endif // Q_OS_OSX
}

OrvWidget::ScrollFlags OrvWidget::calculateScrollFlags(const QPoint& pos) const
{
    if (pos.x() < 0 || pos.y() < 0 || pos.x() >= width() || pos.y() >= height()) {
        return SCROLL_NONE;
    }
    if (scaleFramebuffer()) {
        return SCROLL_NONE;
    }
    const int maxThresholdPixels = 100;
    const double thresholdFraction = 0.33; // if window is very small, use at most this fraction as scroll threshold, otherwise widget will always scroll somewhere
    int hThreshold = std::min((int)(width() * thresholdFraction), maxThresholdPixels);
    int vThreshold = std::min((int)(height() * thresholdFraction), maxThresholdPixels);
    ScrollFlags scrollFlags = SCROLL_NONE;
    if (pos.x() >= 0 && pos.x() <= hThreshold) {
        scrollFlags |= SCROLL_LEFT;
        scrollFlags &= ~SCROLL_RIGHT;
    }
    else if (pos.x() >= width() - hThreshold && pos.x() < width()) {
        scrollFlags |= SCROLL_RIGHT;
        scrollFlags &= ~SCROLL_LEFT;
    }
    if (pos.y() >= 0 && pos.y() <= vThreshold) {
        scrollFlags |= SCROLL_UP;
        scrollFlags &= ~SCROLL_DOWN;
    }
    else if (pos.y() >= height() - vThreshold && pos.y() < height()) {
        scrollFlags |= SCROLL_DOWN;
        scrollFlags &= ~SCROLL_UP;
    }
    return scrollFlags;
}

uint8_t OrvWidget::calculateButtonMask(const QWheelEvent* e)
{
    return calculateButtonMask(e->buttons());
}
uint8_t OrvWidget::calculateButtonMask(const QMouseEvent* e)
{
    return calculateButtonMask(e->buttons());
}
uint8_t OrvWidget::calculateButtonMask(Qt::MouseButtons buttonMask)
{
    uint8_t m = ORV_MOUSE_BUTTON_NONE;
    if (buttonMask & Qt::LeftButton) {
        m |= ORV_MOUSE_BUTTON_LEFT;
    }
    if (buttonMask & Qt::MidButton) {
        m |= ORV_MOUSE_BUTTON_MIDDLE;
    }
    if (buttonMask & Qt::RightButton) {
        m |= ORV_MOUSE_BUTTON_RIGHT;
    }
    return m;
}

void OrvWidget::scrollTimeout()
{
    QPoint pos = mapFromGlobal(QCursor::pos());
    ScrollFlags flags = calculateScrollFlags(pos);
    if (flags == SCROLL_NONE) {
        mScrollTimer->stop();
        return;
    }
    int moveX = 0;
    int moveY = 0;
    if (flags & SCROLL_LEFT) {
        moveX -= SCROLL_STEP;
    }
    else if (flags & SCROLL_RIGHT) {
        moveX += SCROLL_STEP;
    }
    if (flags & SCROLL_UP) {
        moveY -= SCROLL_STEP;
    }
    else if (flags & SCROLL_DOWN) {
        moveY += SCROLL_STEP;
    }
    if (moveX != 0) {
        int newValue = horizontalScrollBar()->value() + moveX;
        newValue = std::max(newValue, 0);
        newValue = std::min(newValue, horizontalScrollBar()->maximum());
        horizontalScrollBar()->setValue(newValue);
    }
    if (moveY != 0) {
        int newValue = verticalScrollBar()->value() + moveY;
        newValue = std::max(newValue, 0);
        newValue = std::min(newValue, verticalScrollBar()->maximum());
        verticalScrollBar()->setValue(newValue);
    }
}


void OrvWidget::updateScrollBars()
{
    if (!scaleFramebuffer()) {
        int fullWidth = mFramebufferWidget->framebufferWidth();
        int fullHeight = mFramebufferWidget->framebufferHeight();
        int remainingWidth = qMax(0, fullWidth - mFramebufferWidget->width());
        int remainingHeight = qMax(0, fullHeight - mFramebufferWidget->height());
        horizontalScrollBar()->setRange(0, remainingWidth);
        verticalScrollBar()->setRange(0, remainingHeight);
        mFramebufferWidget->setFramebufferOffset(horizontalScrollBar()->value(), verticalScrollBar()->value());
    }
}

void OrvWidget::scrollContentsBy(int dx, int dy)
{
    Q_UNUSED(dx);
    Q_UNUSED(dy);
    updateScrollBars();
}

QPoint OrvWidget::mapToFramebuffer(const QPoint& p) const
{
    QPoint ret = p;
    if (!scaleFramebuffer()) {
        ret += QPoint(mFramebufferWidget->framebufferOffsetX(), mFramebufferWidget->framebufferOffsetY());
    }
    else {
        int x = 0;
        if (mFramebufferWidget->width() == 0 || mFramebufferWidget->framebufferWidth() == 0) {
            x = 0;
        }
        else {
            x = (int)((double)p.x() / (double)mFramebufferWidget->width() * (double)mFramebufferWidget->framebufferWidth());
        }
        int y = 0;
        if (mFramebufferWidget->height() == 0 || mFramebufferWidget->framebufferHeight() == 0) {
            y = 0;
        }
        else {
            y = (int)((double)p.y() / (double)mFramebufferWidget->height() * (double)mFramebufferWidget->framebufferHeight());
        }
        ret = QPoint(x, y);
    }
    ret.setX(qMax(0, qMin(ret.x(), (int)mFramebufferWidget->framebufferWidth())));
    ret.setY(qMax(0, qMin(ret.y(), (int)mFramebufferWidget->framebufferHeight())));
    return ret;
}

void OrvWidget::framebufferWidgetInitializedOpenGL()
{
    if (mOrvContext) {
        const orv_framebuffer_t* framebuffer = orv_acquire_framebuffer(mOrvContext);
        if (framebuffer) {
            mFramebufferWidget->initializeFramebuffer(framebuffer);
        }
        orv_release_framebuffer(mOrvContext);
        const orv_cursor_t* cursor = orv_acquire_cursor(mOrvContext);
        if (cursor && cursor->mIsValid) {
            updateCursor(cursor);
        }
        orv_release_cursor(mOrvContext);
    }
}

void OrvWidget::setDebugBitPlanes(int planes)
{
    mFramebufferWidget->setDebugBitPlanes(planes);
}


