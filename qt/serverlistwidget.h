#ifndef SERVERLISTWIDGET_H
#define SERVERLISTWIDGET_H

#include <QWidget>
#include <QAbstractItemDelegate>
#include <QAbstractProxyModel>
#include <QListView>

class ServerListModel;
class ServerListItemDelegate;
class ServerListViewProxyModel;

/**
 * Main widget, displaying the list of available servers (as provided by @ref ServerListModel).
 **/
class ServerListWidget : public QWidget
{
    Q_OBJECT
public:
    ServerListWidget(QWidget* parent = nullptr);
    virtual ~ServerListWidget();

    void setModel(ServerListModel* model);

signals:
    void addNewServer();
    void serverClicked(const QModelIndex& index);
    void editServerClicked(const QModelIndex& index);

protected slots:
    void itemClicked(const QModelIndex& current);

private:
    ServerListModel* mModel = nullptr;
    QListView* mListView = nullptr;
    ServerListItemDelegate* mListViewItemDelegate = nullptr;
    ServerListViewProxyModel* mProxyModel = nullptr;
};

/**
 * Delegate that paints the items of a @ref ServerListModel
 **/
class ServerListItemDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    ServerListItemDelegate(QObject* parent = nullptr);
    virtual ~ServerListItemDelegate();

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool isEditButton(const QRect& itemRect, const QPoint& pos) const;

protected:
    void paintServer(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void paintSeparator(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    QRect makeEditButtonRect(const QRect& itemRect) const;

private:
    QSize mFixedItemSize;
    QSize mFixedSeparatorSize;
    QRect mThumbnailRect;
    int mTextX = 0;
    int mNameTextBottomY = 0;
    int mLastConnectionTextBottomY = 0;
    int mEditButtonLeftRightMargin;
    QFont mNameFont;
    QFont mLastConnectionFont;
    QColor mTextColor;
    QColor mItemBackgroundColor;
    QColor mSeparatorColor;
    QPixmap mEditButtonPixmap;
};

class ServerListViewProxyModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    enum class Columns
    {
        Server = 0,
        EditButton,
        Count // must be last entry
    };
public:
    ServerListViewProxyModel(QObject* parent);
    virtual ~ServerListViewProxyModel();

    virtual void setSourceModel(QAbstractItemModel* sourceModel) override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const override;
    QModelIndex makeEditButtonIndex(int row) const;
    int mapRowFromSource(int sourceRow) const;

protected slots:
    // NOTE: columns of source model never change
    // NOTE: moving rows in source model is not supported
    void sourceModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles = QVector<int>());
    void sourceModelAboutToBeReset();
    void sourceModelReset();
    void sourceModelRowsAboutToBeRemoved(const QModelIndex& parent, int firstRow, int lastRow);
    void sourceModelRowsAboutToBeInserted(const QModelIndex& parent, int firstRow, int lastRow);
    void sourceModelRowsInserted(const QModelIndex& parent, int firstRow, int lastRow);
    void sourceModelRowsRemoved(const QModelIndex& parent, int firstRow, int lastRow);

private:
    ServerListModel* mSourceModel = nullptr;
};

/**
 * Internal QListView class, subclasses for internal reasons.
 **/
class ServerListView : public QListView
{
    Q_OBJECT
public:
    ServerListView(QWidget* parent);
    ~ServerListView();

    virtual QModelIndex indexAt(const QPoint& pos) const override;
};

#endif

