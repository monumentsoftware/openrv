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

#ifndef CONNECTIONINFOWIDGET_H
#define CONNECTIONINFOWIDGET_H

#include <QtWidgets/QScrollArea>

class QTimer;
class QLabel;
class QStackedLayout;

struct orv_communication_pixel_format_t;

class PixelFormatInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PixelFormatInfoWidget(QWidget* parent);

    void setPixelFormat(const orv_communication_pixel_format_t* p);

private:
    QLabel* mBitsPerPixelLabel = nullptr;
    QLabel* mEndiannessLabel = nullptr;
    QLabel* mTrueColorLabel = nullptr;
    QLabel* mRGBMaxLabel = nullptr;
    QLabel* mRGBShiftLabel = nullptr;
};

class ConnectionInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConnectionInfoWidget(QWidget* parent);
    virtual ~ConnectionInfoWidget();

    static QString formattedBytesLong(size_t bytes);

public slots:
    void setOrvContext(struct orv_context_t* context);

protected slots:
    void pollData();

private:
    QTimer* mUpdateTimer = nullptr;
    struct orv_context_t* mOrvContext = nullptr;
    QStackedLayout* mStackedLayout = nullptr;
    QLabel* mNoConnectionLabel = nullptr;
    QWidget* mConnectionInfoWidget = nullptr;
    QLabel* mContextPointerLabel = nullptr;
    QLabel* mIsConnectedLabel = nullptr;
    QLabel* mHostLabel = nullptr;
    QLabel* mPortLabel = nullptr;
    QLabel* mSelectedProtocolVersionLabel = nullptr;
    QLabel* mSelectedSecurityTypeLabel = nullptr;
    QLabel* mFramebufferSizeLabel = nullptr;
    QLabel* mDefaultFramebufferSizeLabel = nullptr;
    QLabel* mDesktopNameLabel = nullptr;
    QLabel* mReceivedLabel = nullptr;
    QLabel* mSentLabel = nullptr;
    PixelFormatInfoWidget* mCommunicationPixelFormat = nullptr;
    PixelFormatInfoWidget* mDefaultPixelFormat = nullptr;
    QLabel* mServerProtocolVersion = nullptr;
    QLabel* mSupportedSecurityTypes = nullptr;
    QLabel* mSupportedEncodingTypes = nullptr;
};

class ScrollableConnectionInfoWidget : public QScrollArea
{
    Q_OBJECT
public:
    explicit ScrollableConnectionInfoWidget(QWidget* parent);

public slots:
    void setOrvContext(struct orv_context_t* context);

private:
    ConnectionInfoWidget* mConnectionInfoWidget = nullptr;
};

#endif

