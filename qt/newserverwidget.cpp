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

#include "newserverwidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QComboBox>
#include <QtGui/QIntValidator>

#define VERTICAL_WIDGET_SPACING 8
#define VERTIACAL_SPACING_IN_ROW 4 // spacing between label and widget
#define DEFAULT_PORT 5900

static const orv_communication_quality_profile_t DEFAULT_QUALITY_PROFILE = ORV_COMM_QUALITY_PROFILE_BEST;

NewServerWidget::NewServerWidget(QWidget* parent)
    : QWidget(parent)
{
    QFont headlineFont;
    headlineFont.setPointSize(20);

    QLabel* headline = new QLabel(tr("New server"), this);
    headline->setFont(headlineFont);

    mHost = new QLineEdit(this);
    mPort = new QLineEdit(this);
    mPort->setValidator(new QIntValidator(this));
    mPassword = new QLineEdit(this);
    mPassword->setEchoMode(QLineEdit::Password);
    mName = new QLineEdit(this);
    mSaveToServerList = new QCheckBox(tr("Save to server-list"), this);
    mSavePassword = new QCheckBox(tr("Save password"), this);
    mConnectImmediately = new QCheckBox(tr("Connect"), this);

    mStatusLabel = new QLabel(this);
    {
        QPalette p;
        p.setColor(QPalette::WindowText,Qt::red);
        mStatusLabel->setPalette(p);
    }
    mStatusLabel->hide();
    mCancel = new QPushButton(tr("&Cancel"), this);
    mDeleteButton = new QPushButton(tr("&Delete"), this);
    mConnectAndSave = new QPushButton(tr("C&onnect"), this);
    QGroupBox* connectionSettings = new QGroupBox(tr("Settings"), this);
    mViewOnly = new QCheckBox(tr("View Only"), connectionSettings);
    QVBoxLayout* settingsLayout = new QVBoxLayout();
    mQualityProfile = new QComboBox(connectionSettings);
    mQualityProfile->addItem(tr("Best quality (high bandwidth)"), qVariantFromValue((int)ORV_COMM_QUALITY_PROFILE_BEST));
    mQualityProfile->addItem(tr("Medium quality (medium bandwidth)"), qVariantFromValue((int)ORV_COMM_QUALITY_PROFILE_MEDIUM));
    mQualityProfile->addItem(tr("Low quality (fastest)"), qVariantFromValue((int)ORV_COMM_QUALITY_PROFILE_LOW));
    mQualityProfile->addItem(tr("Let server decide"), qVariantFromValue((int)ORV_COMM_QUALITY_PROFILE_SERVER));
    //mQualityProfile->addItem(tr("Custom"), qVariantFromValue((int)ORV_COMM_QUALITY_PROFILE_CUSTOM));
    settingsLayout->addWidget(mViewOnly);
    settingsLayout->addWidget(mQualityProfile);
    connectionSettings->setLayout(settingsLayout);


    QVBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->setSpacing(VERTICAL_WIDGET_SPACING);
    topLayout->addWidget(headline);
    topLayout->addLayout(makeRow(tr("Host:"), mHost));
    topLayout->addLayout(makeRow(tr("Port:"), mPort));
    topLayout->addLayout(makeRow(tr("Password:"), mPassword));
    topLayout->addLayout(makeRow(tr("Name (optional):"), mName, &mNameLabel));
    topLayout->addWidget(mSaveToServerList);
    topLayout->addWidget(mSavePassword);
    topLayout->addWidget(mConnectImmediately);
    topLayout->addWidget(connectionSettings);
    topLayout->addWidget(mStatusLabel);
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(mCancel);
    buttonLayout->addWidget(mDeleteButton);
    buttonLayout->addWidget(mConnectAndSave);
    topLayout->addLayout(buttonLayout);
    topLayout->addStretch();
    updateConnectButtonText();

    connect(mSaveToServerList, SIGNAL(toggled(bool)), this, SLOT(saveToServerListChanged(bool)));
    connect(mSavePassword, SIGNAL(toggled(bool)), this, SLOT(savePasswordChanged(bool)));
    connect(mConnectImmediately, SIGNAL(toggled(bool)), this, SLOT(connectImmediatelyChanged(bool)));
    connect(mCancel, SIGNAL(clicked()), this, SIGNAL(cancelWidget()));
    connect(mDeleteButton, SIGNAL(clicked()), this, SLOT(deleteServer()));
    connect(mConnectAndSave, SIGNAL(clicked()), this, SLOT(checkAndConnectOrSave()));

    reset();
}

NewServerWidget::~NewServerWidget()
{
}

QLayout* NewServerWidget::makeRow(const QString& label, QWidget* widget, QLabel** labelPointer)
{
    QVBoxLayout* l = new QVBoxLayout();
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(VERTIACAL_SPACING_IN_ROW);
    QLabel* labelWidget = new QLabel(label, this);
    l->addWidget(labelWidget);
    l->addWidget(widget);
    if (labelPointer) {
        *labelPointer = labelWidget;
    }
    return l;
}

void NewServerWidget::updateConnectButtonText()
{
    bool connect = mConnectImmediately->isChecked();
    if (!mConnectImmediately->isChecked() && !mSaveToServerList->isChecked()) {
        mConnectAndSave->setEnabled(false);
        connect = true;
    }
    else {
        mConnectAndSave->setEnabled(true);
    }
    if (connect && mSaveToServerList->isChecked()) {
        mConnectAndSave->setText(tr("Connect and save"));
    }
    else if (connect) {
        mConnectAndSave->setText(tr("Connect"));
    }
    else {
        mConnectAndSave->setText(tr("Save"));
    }
}

void NewServerWidget::saveToServerListChanged(bool enabled)
{
    mSavePassword->setEnabled(enabled);
    mNameLabel->setEnabled(enabled);
    mName->setEnabled(enabled);
    updateConnectButtonText();
}

void NewServerWidget::savePasswordChanged(bool enabled)
{
    Q_UNUSED(enabled);
}

void NewServerWidget::connectImmediatelyChanged(bool enabled)
{
    Q_UNUSED(enabled);
    updateConnectButtonText();
}

/**
 * Check the user input for correctness and start the connection and/or save the server.
 **/
void NewServerWidget::checkAndConnectOrSave()
{
    mStatusLabel->hide();
    if (mHost->text().isEmpty()) {
        mStatusLabel->setText(tr("Please enter a hostname first"));
        mStatusLabel->show();
        return;
    }
    if (mPort->text().isEmpty()) {
        mStatusLabel->setText(tr("Please enter a port number first"));
        mStatusLabel->show();
        return;
    }
    bool ok = true;
    int port = mPort->text().toInt(&ok);
    if (!ok) {
        mStatusLabel->setText(tr("Port must be a number"));
        mStatusLabel->show();
        return;
    }
    if (port <= 0 || port > 65535) {
        mStatusLabel->setText(tr("Port number is not in valid range"));
        mStatusLabel->show();
        return;
    }
    const orv_communication_quality_profile_t qualityProfile = (orv_communication_quality_profile_t)mQualityProfile->currentData().toInt();
    orv_communication_pixel_format_t customPixelFormat; // only relevant if qualityProfile is ORV_COMM_QUALITY_PROFILE_CUSTOM
    getCustomPixelFormat(&customPixelFormat);
    if (mSaveToServerList->isChecked()) {
        QString password = mPassword->text();
        if (!mSavePassword->isChecked()) {
            password = QString();
        }
        // NOTE: receiver will NOT close the widget
        emit saveToServerList(mHost->text(), port, password, mName->text(), mSavePassword->isChecked(), mInternalServerId, mViewOnly->isChecked(), qualityProfile, customPixelFormat);
    }

    // NOTE: receiver should close the widget
    if (mConnectImmediately->isChecked()) {
        emit connectToHost(mHost->text(), port, mPassword->text(), mViewOnly->isChecked(), qualityProfile, customPixelFormat, mInternalServerId);
    }
    else {
        emit cancelWidget();
    }
}

void NewServerWidget::deleteServer()
{
    if (mInternalServerId <= 0) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete server"), tr("Really delete this server from config?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        emit deleteServerAndCancelWidget(mInternalServerId);
    }
}

void NewServerWidget::setHostName(const QString& hostName)
{
    mHost->setText(hostName);
}

void NewServerWidget::setPort(int port)
{
    mPort->setText(QString::number(port));
}

void NewServerWidget::setPassword(const QString& password)
{
    mPassword->setText(password);
}

void NewServerWidget::setName(const QString& name)
{
    mName->setText(name);
}

void NewServerWidget::setConnectImmediately(bool connect)
{
    mConnectImmediately->setChecked(connect);
}

void NewServerWidget::setSaveToServerList(bool save)
{
    mSaveToServerList->setChecked(save);
}

void NewServerWidget::setSavePassword(bool save)
{
    mSavePassword->setChecked(save);
}

void NewServerWidget::setInternalServerId(int id)
{
    mInternalServerId = id;
    if (mInternalServerId > 0) {
        mDeleteButton->show();
    }
    else {
        mDeleteButton->hide();
    }
}

void NewServerWidget::setViewOnly(bool viewOnly)
{
    mViewOnly->setChecked(viewOnly);
}

void NewServerWidget::setQualityProfile(orv_communication_quality_profile_t qualityProfile)
{
    int index = mQualityProfile->findData(qVariantFromValue((int)qualityProfile));
    if (index >= 0) {
        mQualityProfile->setCurrentIndex(index);
    }
    else {
        mQualityProfile->setCurrentIndex(mQualityProfile->findData(qVariantFromValue((int)DEFAULT_QUALITY_PROFILE)));
    }
}

void NewServerWidget::setCustomPixelFormat(const orv_communication_pixel_format_t& format)
{
#warning TODO
}

void NewServerWidget::reset()
{
    mInternalServerId = -1;
    mHost->setText(QString());
    mPort->setText(QString::number(DEFAULT_PORT));
    mPassword->setText(QString());
    mConnectImmediately->setChecked(true);
    mSaveToServerList->setChecked(true);
    mSavePassword->setChecked(true);
    mName->setText(QString());
    mStatusLabel->hide();
    mHost->setFocus();
    mDeleteButton->hide();
    mViewOnly->setChecked(false);
    mQualityProfile->setCurrentIndex(mQualityProfile->findData(qVariantFromValue((int)DEFAULT_QUALITY_PROFILE)));
    updateConnectButtonText();
    saveToServerListChanged(mSaveToServerList->isChecked());
}

QString NewServerWidget::hostName() const
{
    return mHost->text();
}

int NewServerWidget::port() const
{
    bool ok;
    int port = mPort->text().toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        return 5900;
    }
    return port;
}

QString NewServerWidget::password() const
{
    return mPassword->text();
}

QString NewServerWidget::name() const
{
    return mPassword->text();
}

bool NewServerWidget::connectImmediately() const
{
    return mConnectImmediately->isChecked();
}

bool NewServerWidget::saveToServerList() const
{
    return mSaveToServerList->isChecked();
}

bool NewServerWidget::savePassword() const
{
    return mSavePassword->isChecked();
}

int NewServerWidget::internalServerId() const
{
    return mInternalServerId;
}
bool NewServerWidget::viewOnly() const
{
    return mViewOnly->isChecked();
}

orv_communication_quality_profile_t NewServerWidget::qualityProfile() const
{
    return (orv_communication_quality_profile_t)mQualityProfile->currentData().toInt();
}

void NewServerWidget::getCustomPixelFormat(orv_communication_pixel_format_t* format) const
{
#warning TODO
}

