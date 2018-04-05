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

#include "connectstatewidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>

ConnectStateWidget::ConnectStateWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    mHeader = new QLabel(this);
    layout->addWidget(mHeader);
    mError = new QLabel(this);
    layout->addWidget(mError);
    mAbort = new QPushButton(tr("&Abort"), this);
    layout->addWidget(mAbort);
    layout->addStretch();

    connect(mAbort, SIGNAL(clicked()), this, SIGNAL(abort()));
}

ConnectStateWidget::~ConnectStateWidget()
{
}

void ConnectStateWidget::setHost(const QString& host, int port)
{
    mHost = host;
    mPort = port;
    mHeader->setText(tr("Connecting to %1:%2...").arg(mHost).arg(mPort));
}

void ConnectStateWidget::setError(const QString& error)
{
    mHeader->setText(tr("Failed to connect to %1:%2...").arg(mHost).arg(mPort));
    mError->setText(tr("Error: %1").arg(error));
}

void ConnectStateWidget::clear()
{
    mHost = QString();
    mPort = 0;
    mHeader->setText(QString());
    mError->setText(QString());
}

