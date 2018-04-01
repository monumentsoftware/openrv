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

