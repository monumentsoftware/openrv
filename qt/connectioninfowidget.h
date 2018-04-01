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

