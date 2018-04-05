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

#include "connectioninfowidget.h"

#include <QtCore/QTimer>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGroupBox>

#include <libopenrv/libopenrv.h>

PixelFormatInfoWidget::PixelFormatInfoWidget(QWidget* parent)
    : QWidget(parent)
{
    QFormLayout* pFormatLayout = new QFormLayout(this);
    mBitsPerPixelLabel = new QLabel(this);
    pFormatLayout->addRow(tr("Bits per Pixel"), mBitsPerPixelLabel);
    mTrueColorLabel = new QLabel(this);
    pFormatLayout->addRow(tr("TrueColor"), mTrueColorLabel);
    mEndiannessLabel = new QLabel(this);
    pFormatLayout->addRow(tr("Endianness"), mEndiannessLabel);
    mRGBMaxLabel = new QLabel(this);
    pFormatLayout->addRow(tr("R/G/B Max"), mRGBMaxLabel);
    mRGBShiftLabel = new QLabel(this);
    pFormatLayout->addRow(tr("R/G/B Shift"), mRGBShiftLabel);
}

void PixelFormatInfoWidget::setPixelFormat(const orv_communication_pixel_format_t* p)
{
    QString bitsPerPixel;
    QString endianness;
    QString trueColor;
    QString rgbMax;
    QString rgbShift;
    if (p) {
        bitsPerPixel = tr("%1 (%2 used)").arg(p->mBitsPerPixel).arg(p->mDepth);
        endianness = p->mBigEndian ? tr("Big Endian") : tr("Little Endian");
        trueColor = p->mTrueColor ? tr("Yes") : tr("No");
        rgbMax = tr("%1/%2/%3").arg(p->mColorMax[0]).arg(p->mColorMax[1]).arg(p->mColorMax[2]);
        rgbShift = tr("%1/%2/%3").arg(p->mColorShift[0]).arg(p->mColorShift[1]).arg(p->mColorShift[2]);
    }
    mBitsPerPixelLabel->setText(bitsPerPixel);
    mEndiannessLabel->setText(endianness);
    mTrueColorLabel->setText(trueColor);
    mRGBMaxLabel->setText(rgbMax);
    mRGBShiftLabel->setText(rgbShift);
}

ConnectionInfoWidget::ConnectionInfoWidget(QWidget* parent)
    : QWidget(parent)
{
    mUpdateTimer = new QTimer(this);
    connect(mUpdateTimer, SIGNAL(timeout()), this, SLOT(pollData()));

    mStackedLayout = new QStackedLayout(this);
    mNoConnectionLabel = new QLabel(tr("Have no OpenRV context"), this);
    mStackedLayout->addWidget(mNoConnectionLabel);

    mConnectionInfoWidget = new QWidget(this);
    mStackedLayout->addWidget(mConnectionInfoWidget);
    QFormLayout* layout = new QFormLayout(mConnectionInfoWidget);
    mContextPointerLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("OpenRV Context"), mContextPointerLabel);
    mIsConnectedLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Is Connected"), mIsConnectedLabel);
    mHostLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Hostname"), mHostLabel);
    mPortLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Port"), mPortLabel);
    mSelectedProtocolVersionLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Protocol"), mSelectedProtocolVersionLabel);
    mSelectedSecurityTypeLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Security Type"), mSelectedSecurityTypeLabel);
    mDefaultFramebufferSizeLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Default Framebuffer Size"), mDefaultFramebufferSizeLabel);
    mFramebufferSizeLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Framebuffer Size"), mFramebufferSizeLabel);
    mDesktopNameLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Desktop Name"), mDesktopNameLabel);
    mReceivedLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Received"), mReceivedLabel);
    mSentLabel = new QLabel(mConnectionInfoWidget);
    layout->addRow(tr("Sent"), mSentLabel);

    QGroupBox* pixelFormatBox = new QGroupBox(tr("Communication Pixel Format"), this);
    layout->addRow(pixelFormatBox);
    QVBoxLayout* pixelFormatBoxLayout = new QVBoxLayout(pixelFormatBox);
    pixelFormatBox->setContentsMargins(QMargins());
    mCommunicationPixelFormat = new PixelFormatInfoWidget(pixelFormatBox);
    pixelFormatBoxLayout->addWidget(mCommunicationPixelFormat);

    QGroupBox* defaultPixelFormatBox = new QGroupBox(tr("Default Pixel Format (advertised by server)"), this);
    layout->addRow(defaultPixelFormatBox);
    QVBoxLayout* defaultPixelFormatBoxLayout = new QVBoxLayout(defaultPixelFormatBox);
    defaultPixelFormatBox->setContentsMargins(QMargins());
    mDefaultPixelFormat = new PixelFormatInfoWidget(defaultPixelFormatBox);
    defaultPixelFormatBoxLayout->addWidget(mDefaultPixelFormat);

    QGroupBox* serverCapabilitiesBox = new QGroupBox(tr("Server Capabilities"), this);
    layout->addRow(serverCapabilitiesBox);
    QFormLayout* capLayout = new QFormLayout(serverCapabilitiesBox);
    mServerProtocolVersion = new QLabel(serverCapabilitiesBox);
    capLayout->addRow(tr("Server Protocol Version"), mServerProtocolVersion);
    mSupportedSecurityTypes = new QLabel(serverCapabilitiesBox);
    capLayout->addRow(tr("Supported Security Types"), mSupportedSecurityTypes);
    mSupportedEncodingTypes = new QLabel(serverCapabilitiesBox);
    capLayout->addRow(tr("Supported Encoding Types"), mSupportedEncodingTypes);

    mStackedLayout->setCurrentWidget(mNoConnectionLabel);
}

ConnectionInfoWidget::~ConnectionInfoWidget()
{
}

/**
 * Make this widget display data for @p context. If @p context is NULL, no data is displayed by this
 * widget, any previously set context is removed.
 *
 * The provided pointer must remain valid for the lifetime of this object or until this function is
 * called again with a different pointer.
 **/
void ConnectionInfoWidget::setOrvContext(struct orv_context_t* context)
{
    if (!context) {
        mUpdateTimer->stop();
        mStackedLayout->setCurrentWidget(mNoConnectionLabel);
        return;
    }
    mOrvContext = context;
    const int pollTimeoutMs = 1000;
    mUpdateTimer->start(pollTimeoutMs);
    mStackedLayout->setCurrentWidget(mConnectionInfoWidget);
    pollData();
}

void ConnectionInfoWidget::pollData()
{
    if (!mOrvContext) {
        setOrvContext(nullptr);
        return;
    }

    // TODO: do not query capabilities in *every* iteration, querying info is enough. capabilities
    //       normally does not change (and never does if the "partial" flag of the encodings is 0)
    bool fetchServerCapabilities = true;
    orv_connection_info_t info;
    orv_vnc_server_capabilities_t capabilitiesData;
    orv_vnc_server_capabilities_t* capabilities = nullptr;
    orv_connection_info_reset(&info);
    if (fetchServerCapabilities) {
        capabilities = &capabilitiesData;
        orv_vnc_server_capabilities_reset(capabilities);
    }
    orv_get_vnc_connection_info(mOrvContext, &info, capabilities);

    QString s;
    s.sprintf("%p", mOrvContext);
    mContextPointerLabel->setText(s);
    mIsConnectedLabel->setText((info.mConnected == 1) ? tr("Yes") : tr("No"));
    mHostLabel->setText(QString::fromLocal8Bit(info.mHostName));
    mPortLabel->setText(QString::number(info.mPort));
    mReceivedLabel->setText(formattedBytesLong(info.mReceivedBytes));
    mSentLabel->setText(formattedBytesLong(info.mSentBytes));
    QString selectedProtocolVersion;
    QString selectedSecurityType;
    QString framebufferSize;
    QString defaultFramebufferSize;
    QString desktopName;
    const orv_communication_pixel_format_t* communicationPixelFormat = nullptr;
    const orv_communication_pixel_format_t* defaultPixelFormat = nullptr;
    if (info.mConnected == 1) {
        selectedProtocolVersion = QString::fromLatin1(info.mSelectedProtocolVersionString).simplified();
        selectedSecurityType  = tr("%1 (%2)").arg(info.mSelectedVNCSecurityType).arg(orv_get_vnc_security_type_string(info.mSelectedVNCSecurityType));
        framebufferSize = tr("%1x%2").arg(info.mFramebufferWidth).arg(info.mFramebufferHeight);
        defaultFramebufferSize = tr("%1x%2").arg(info.mDefaultFramebufferWidth).arg(info.mDefaultFramebufferHeight);
        desktopName = QString::fromUtf8(info.mDesktopName);
        communicationPixelFormat = &info.mCommunicationPixelFormat;
        defaultPixelFormat = &info.mDefaultPixelFormat;
    }
    mSelectedProtocolVersionLabel->setText(selectedProtocolVersion);
    mSelectedSecurityTypeLabel->setText(selectedSecurityType);
    mFramebufferSizeLabel->setText(framebufferSize);
    mDefaultFramebufferSizeLabel->setText(defaultFramebufferSize);
    mDesktopNameLabel->setText(desktopName);
    mCommunicationPixelFormat->setPixelFormat(communicationPixelFormat);
    mDefaultPixelFormat->setPixelFormat(defaultPixelFormat);

    if (capabilities) {
        mServerProtocolVersion->setText(QString::fromLatin1(capabilities->mServerProtocolVersionString).simplified());
        QString securityTypes;
        for (int i = 0; i < (int)capabilities->mSupportedSecurityTypesCount; i++) {
            uint8_t securityType = capabilities->mSupportedSecurityTypes[i];
            securityTypes += tr("%1 (%2)\n").arg(QString::number(securityType)).arg(orv_get_vnc_security_type_string(securityType));
        }
        securityTypes += tr("(Total: %1 types)").arg(capabilities->mSupportedSecurityTypesCount);
        mSupportedSecurityTypes->setText(securityTypes);
        QString encodingTypes;
        for (int i = 0; i < (int)capabilities->mSupportedEncodingCapabilitiesCount; i++) {
            int32_t encodingType = capabilities->mSupportedEncodingCapabilities[i].mCode;
            const char* vendor = (const char*)capabilities->mSupportedEncodingCapabilities[i].mVendor;
            const char* signature = (const char*)capabilities->mSupportedEncodingCapabilities[i].mSignature;
            encodingTypes += tr("%1 (vendor: %2, signature: %3) - %4\n").arg(QString::number(encodingType)).arg(QString::fromLatin1(vendor)).arg(QString::fromLatin1(signature)).arg(orv_get_vnc_encoding_type_string(encodingType));
        }
        encodingTypes += tr("(Total: %1 types)").arg(capabilities->mSupportedEncodingCapabilitiesCount);
        if (capabilities->mSupportedEncodingCapabilitiesPartial) {
            encodingTypes += tr("\n(Partial list only, no full capabilities from server available)");
        }
        mSupportedEncodingTypes->setText(encodingTypes);
    }


    // TODO:
    // - display current clipboard contents?
    //   -> currently provided via events only though
}

/**
 * @return A human readable representation of @p bytes as KiB/MiB/etc. including the original full
 *         length value in braces.
 **/
QString ConnectionInfoWidget::formattedBytesLong(size_t bytes)
{
    if (bytes < 1024) {
        return tr("%1 Bytes").arg(bytes);
    }
    else if (bytes < 1024 * 1024) {
        double b = (double)bytes / 1024.0;
        return tr("%1 KiB (%2 Bytes)").arg(b, 0, 'f', 2).arg(bytes);
    }
    else if (bytes < 1024 * 1024 * 1024) {
        double b = (double)bytes / (1024.0 * 1024);
        return tr("%1 MiB (%2 Bytes)").arg(b, 0, 'f', 2).arg(bytes);
    }
    else if (bytes < 1024 * 1024 * 1024) {
        double b = (double)bytes / (1024.0 * 1024);
        return tr("%1 MiB (%2 Bytes)").arg(b, 0, 'f', 2).arg(bytes);
    }
    else {
        double b = (double)bytes / (1024.0 * 1024 * 1024);
        return tr("%1 GiB (%2 Bytes)").arg(b, 0, 'f', 2).arg(bytes);
    }
}


ScrollableConnectionInfoWidget::ScrollableConnectionInfoWidget(QWidget* parent)
    : QScrollArea(parent)
{
    mConnectionInfoWidget = new ConnectionInfoWidget(this);
    setWidget(mConnectionInfoWidget);
    setWidgetResizable(true);
}

void ScrollableConnectionInfoWidget::setOrvContext(struct orv_context_t* context)
{
    mConnectionInfoWidget->setOrvContext(context);
}

