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

