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

#ifndef CONNECTSTATEWIDGET_H
#define CONNECTSTATEWIDGET_H

#include <QtWidgets/QWidget>

class QPushButton;
class QLabel;

class ConnectStateWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConnectStateWidget(QWidget* parent);
    virtual ~ConnectStateWidget();

    void setHost(const QString& host, int port);
    void setError(const QString& error);
    void clear();

signals:
    void abort();

private:
    QString mHost;
    int mPort = 0;
    QLabel* mHeader = nullptr;
    QLabel* mError = nullptr;
    QPushButton* mAbort = nullptr;
};

#endif

