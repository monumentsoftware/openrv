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

#ifndef NEWSERVERWIDGET_H
#define NEWSERVERWIDGET_H

#include <QtWidgets/QWidget>

#include <libopenrv/libopenrv.h>

class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QComboBox;

class NewServerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NewServerWidget(QWidget* parent);
    virtual ~NewServerWidget();

    void setHostName(const QString& hostName);
    void setPort(int port);
    void setPassword(const QString& password);
    void setName(const QString& name);
    void setConnectImmediately(bool connect);
    void setSaveToServerList(bool save);
    void setSavePassword(bool save);
    void setInternalServerId(int id);
    void setViewOnly(bool viewOnly);
    void setQualityProfile(orv_communication_quality_profile_t qualityProfile);
    void setCustomPixelFormat(const orv_communication_pixel_format_t& format);

    void reset();

    QString hostName() const;
    int port() const;
    QString password() const;
    QString name() const;
    bool connectImmediately() const;
    bool saveToServerList() const;
    bool savePassword() const;
    int internalServerId() const;

    bool viewOnly() const;
    orv_communication_quality_profile_t qualityProfile() const;
    void getCustomPixelFormat(orv_communication_pixel_format_t* format) const;

public slots:
    void checkAndConnectOrSave();

signals:
    /**
     * @param internalServerId The internal ID of the server for the settings file. This can be used
     *        to uniquely identify a server. -1 to add a new connection.
     **/
    void saveToServerList(const QString& host, int port, const QString& password, const QString& name, bool savePassword, int internalServerId, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPxelFormat);
    void connectToHost(const QString& host, int port, const QString& password, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat, int internalServerId);
    void deleteServerAndCancelWidget(int internalServerId);
    void cancelWidget();

protected slots:
    void saveToServerListChanged(bool enabled);
    void savePasswordChanged(bool enabled);
    void connectImmediatelyChanged(bool enabled);
    void deleteServer();

protected:
    void updateConnectButtonText();
    QLayout* makeRow(const QString& label, QWidget* widget, QLabel** labelPointer = nullptr);

private:
    QLineEdit* mHost = nullptr;
    QLineEdit* mPort = nullptr;
    QLineEdit* mPassword = nullptr;
    QLabel* mNameLabel = nullptr;
    QLineEdit* mName = nullptr;
    QPushButton* mCancel = nullptr;
    QPushButton* mDeleteButton = nullptr;
    QPushButton* mConnectAndSave = nullptr;
    QLabel* mStatusLabel = nullptr;
    QCheckBox* mSaveToServerList = nullptr;
    QCheckBox* mSavePassword = nullptr;
    QCheckBox* mConnectImmediately = nullptr;
    QCheckBox* mViewOnly = nullptr;
    QComboBox* mQualityProfile;
    int mInternalServerId = -1;
};

#endif

