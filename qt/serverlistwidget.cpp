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

#include "serverlistwidget.h"
#include "serverlistmodel.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QApplication>
#include <QtGui/QPixmap>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtCore/QtDebug>

ServerListWidget::ServerListWidget(QWidget* parent)
    : QWidget(parent)
{
    QFont headlineFont;
    headlineFont.setPointSize(20);

    QLabel* headline = new QLabel(tr("Server list"), this);
    headline->setFont(headlineFont);

    mListViewItemDelegate = new ServerListItemDelegate(this);
    mListView = new ServerListView(this);
    mListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mListView->setItemDelegate(mListViewItemDelegate);
    mProxyModel = new ServerListViewProxyModel(this);
    mListView->setModel(mProxyModel);
    mListView->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(mListView, SIGNAL(clicked(const QModelIndex&)), this, SLOT(itemClicked(const QModelIndex&)));

    QPushButton* newServer = new QPushButton(tr("New server"), this);
    connect(newServer, SIGNAL(clicked()), this, SIGNAL(addNewServer()));

    QVBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->addWidget(headline);
    topLayout->addWidget(mListView, 1);
    topLayout->addWidget(newServer);

}

ServerListWidget::~ServerListWidget()
{
}

void ServerListWidget::setModel(ServerListModel* model)
{
    mModel = model;
    mProxyModel->setSourceModel(mModel);
}

void ServerListWidget::itemClicked(const QModelIndex& proxyIndex)
{
    if (!proxyIndex.isValid()) {
        return;
    }
    QModelIndex serverIndex = mProxyModel->mapToSource(proxyIndex);
    if (!serverIndex.isValid()) {
        return;
    }
    switch ((ServerListViewProxyModel::Columns)proxyIndex.column()) {
        case ServerListViewProxyModel::Columns::Server:
            emit serverClicked(serverIndex);
            break;
        case ServerListViewProxyModel::Columns::EditButton:
            emit editServerClicked(serverIndex);
            break;
        case ServerListViewProxyModel::Columns::Count:
            break;
    }
}


ServerListItemDelegate::ServerListItemDelegate(QObject* parent)
    : QAbstractItemDelegate(parent)
{
    const int thumbnailTopBottomMargin = 10;
    const int thumbnailLeftRightMargin = 10;
    const int thumbnailWidth = ServerListModel::mServerThumbnailWidth;
    const int thumbnailHeight = ServerListModel::mServerThumbnailHeight;
    const int textTopBottomMargin = 10;
    const int rightMargin = thumbnailLeftRightMargin;
    mThumbnailRect = QRect(thumbnailLeftRightMargin, thumbnailTopBottomMargin, thumbnailWidth, thumbnailHeight);
    const int minTextSize = 50;
    const int textLineSpacing = 5;

    const int editButtonWidth = 30;
    const int editButtonHeight = 30;
    {
        QImage image(editButtonWidth, editButtonHeight, QImage::Format_ARGB32);
        image.fill(qRgba(255, 0, 0, 255));
        mEditButtonPixmap = QPixmap::fromImage(image);
    }
    mEditButtonLeftRightMargin = 10;

    mNameFont.setPointSize(22);
    mLastConnectionFont.setPointSize(12);

    mTextX = thumbnailLeftRightMargin * 2 + thumbnailWidth;
    mNameTextBottomY = textTopBottomMargin + QFontMetrics(mNameFont).height();
    mLastConnectionTextBottomY = mNameTextBottomY + QFontMetrics(mLastConnectionFont).height() + textLineSpacing;
    const int width = mTextX + minTextSize + rightMargin + mEditButtonPixmap.width() + 2*mEditButtonLeftRightMargin;
    const int height = qMax(thumbnailTopBottomMargin * 2 + thumbnailHeight,
            qMax(mLastConnectionTextBottomY + textTopBottomMargin, mEditButtonPixmap.height()));
    mFixedItemSize = QSize(width, height);

    mFixedSeparatorSize = QSize(width, 2);

    mTextColor = QColor(255, 255, 255);
    mItemBackgroundColor = QColor(111, 152, 175);
    mSeparatorColor = QColor(255, 255, 255);
}

ServerListItemDelegate::~ServerListItemDelegate()
{
}

void ServerListItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid()) {
        return;
    }
    ServerListModel::ItemType itemType = (ServerListModel::ItemType)index.data((int)ServerListModel::Roles::ItemType).toInt();
    painter->save();
    painter->setClipRect(option.rect);
    switch (itemType) {
        case ServerListModel::ItemType::Server:
            paintServer(painter, option, index);
            break;
        case ServerListModel::ItemType::Separator:
            paintSeparator(painter, option, index);
            break;
    }
    painter->restore();
}

void ServerListItemDelegate::paintServer(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (option.state & QStyle::State_Selected) {
        // background?
    }
    painter->fillRect(option.rect, QBrush(mItemBackgroundColor));
    QPixmap thumbnail;
    QString name = index.data((int)ServerListModel::Roles::Name).toString();
    if (name.isEmpty()) {
        name = index.data((int)ServerListModel::Roles::HostName).toString();
    }
    if (name.isEmpty()) {
        name = tr("(unknown)");
    }
    QDateTime lastConnectionDateTime = index.data((int)ServerListModel::Roles::LastConnection).toDateTime();
    QString lastConnection;
    if (lastConnectionDateTime.isNull()) {
        lastConnection = tr("No connection yet");
    }
    else {
        lastConnection = lastConnectionDateTime.toString(Qt::TextDate);
    }

    if (thumbnail.isNull()) {
        QRect thumbnailRect = mThumbnailRect;
        thumbnailRect.translate(option.rect.topLeft());
        painter->drawPixmap(thumbnailRect, thumbnail); // NOTE: scales, if required!
    }

    const int textX = option.rect.left() + mTextX;
    painter->setPen(QPen(mTextColor));
    painter->setFont(mNameFont);
    painter->drawText(textX, option.rect.top() + mNameTextBottomY - painter->fontMetrics().descent()-1, name);
    painter->setFont(mLastConnectionFont);
    painter->drawText(textX, option.rect.top() + mLastConnectionTextBottomY - painter->fontMetrics().descent()-1, lastConnection);

    QRect editButtonRect = makeEditButtonRect(option.rect);
    editButtonRect.translate(option.rect.topLeft());
    painter->drawPixmap(editButtonRect, mEditButtonPixmap);
}

void ServerListItemDelegate::paintSeparator(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index);
    QBrush brush(Qt::white);
    painter->fillRect(option.rect, mSeparatorColor);
}

QRect ServerListItemDelegate::makeEditButtonRect(const QRect& itemRect) const
{
    // NOTE: size of rect must match pixmap, otherwise drawPixmap() will scale
    return QRect(
        itemRect.width() - mEditButtonPixmap.width() - mEditButtonLeftRightMargin,
        (itemRect.height() - mEditButtonPixmap.height()) / 2,
        mEditButtonPixmap.width(),
        mEditButtonPixmap.height()
    );
}

QSize ServerListItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    if (!index.isValid()) {
        return QSize();
    }
    ServerListModel::ItemType itemType = (ServerListModel::ItemType)index.data((int)ServerListModel::Roles::ItemType).toInt();
    switch (itemType) {
        case ServerListModel::ItemType::Server:
            return mFixedItemSize;
        case ServerListModel::ItemType::Separator:
            return mFixedSeparatorSize;
    }
    return QSize();
}

/**
 * @return TRUE if @p pos is inside the edit button,
 *         otherwise FALSE. The @p itemRect must be the rect of the item that is used for painting.
 *         Both, @p pos and @p itemRect must use the same coordinate system.
 **/
bool ServerListItemDelegate::isEditButton(const QRect& itemRect, const QPoint& pos) const
{
    QPoint relativePos = QPoint(pos.x() - itemRect.left(), pos.y() - itemRect.top());
    QRect editButtonRect = makeEditButtonRect(itemRect);
    if (editButtonRect.contains(relativePos)) {
        return true;
    }
    return false;
}


ServerListViewProxyModel::ServerListViewProxyModel(QObject* parent)
    : QAbstractProxyModel(parent)
{
}

ServerListViewProxyModel::~ServerListViewProxyModel()
{
}

void ServerListViewProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    ServerListModel* m = qobject_cast<ServerListModel*>(sourceModel);
    beginResetModel();
    if (mSourceModel) {
        disconnect(mSourceModel, 0, this, 0);
    }
    QAbstractProxyModel::setSourceModel(m);
    mSourceModel = m;
    if (mSourceModel) {
        connect(mSourceModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)), this, SLOT(sourceModelDataChanged(const QModelIndex&, const QModelIndex, const QVector<int>&)));
        connect(mSourceModel, SIGNAL(modelAboutToBeReset()), this, SLOT(sourceModelAboutToBeReset()));
        connect(mSourceModel, SIGNAL(modelReset()), this, SLOT(sourceModelReset()));
        connect(mSourceModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)), this, SLOT(sourceModelRowsAboutToBeRemoved(const QModelIndex&, int, int)));
        connect(mSourceModel, SIGNAL(rowsAboutToBeInserted(const QModelIndex&, int, int)), this, SLOT(sourceModelRowsAboutToBeInserted(const QModelIndex&, int, int)));
        connect(mSourceModel, SIGNAL(rowsRemoved(const QModelIndex&, int, int)), this, SLOT(sourceModelRowsRemoved(const QModelIndex&, int, int)));
        connect(mSourceModel, SIGNAL(rowsInserted(const QModelIndex&, int, int)), this, SLOT(sourceModelRowsInserted(const QModelIndex&, int, int)));
    }
    endResetModel();
}

QModelIndex ServerListViewProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!mSourceModel) {
        return QModelIndex();
    }
    if (!sourceIndex.isValid()) {
        return QModelIndex();
    }
    if (sourceIndex.parent().isValid() || sourceIndex.column() != 0) {
        return QModelIndex();
    }
    if (sourceIndex.row() >= mSourceModel->rowCount()) {
        return QModelIndex();
    }
    return index(mapRowFromSource(sourceIndex.row()), (int)Columns::Server, QModelIndex());
}

int ServerListViewProxyModel::mapRowFromSource(int sourceRow) const
{
    if (sourceRow < 0) {
        return 0;
    }
    return sourceRow * 2;
}

QModelIndex ServerListViewProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!mSourceModel) {
        return QModelIndex();
    }
    if (!proxyIndex.isValid()) {
        return QModelIndex();
    }
    if (proxyIndex.parent().isValid()) {
        return QModelIndex();
    }
    int row = proxyIndex.row();
    if (row % 2 != 0) {
        // separator item. this has no match in source model.
        return QModelIndex();
    }
    if (row >= rowCount()) {
        return QModelIndex();
    }
    return mSourceModel->index(row / 2, 0, QModelIndex());
}

int ServerListViewProxyModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    if (!mSourceModel) {
        return 0;
    }
    return (int)Columns::Count;
}

int ServerListViewProxyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    if (!mSourceModel) {
        return 0;
    }
    int count = mSourceModel->rowCount();
    if (count <= 1) {
        return count;
    }
    return count + (count - 1);
}

QModelIndex ServerListViewProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return QModelIndex();
    }
    if (row < 0 || column < 0) {
        return QModelIndex();
    }
    if (row >= rowCount() || column >= columnCount()) {
        return QModelIndex();
    }
    return createIndex(row, column);
}

QModelIndex ServerListViewProxyModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

QVariant ServerListViewProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
    if (role == (int)ServerListModel::Roles::ItemType) {
        if (proxyIndex.isValid() && !proxyIndex.parent().isValid() && (proxyIndex.row() % 2 == 1)) {
            return qVariantFromValue((int)ServerListModel::ItemType::Separator);
        }
    }
    return QAbstractProxyModel::data(proxyIndex, role);
}

QModelIndex ServerListViewProxyModel::makeEditButtonIndex(int row) const
{
    return index(row, (int)Columns::EditButton);
}

void ServerListViewProxyModel::sourceModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles)
{
    QModelIndex myTopLeft = mapFromSource(topLeft);
    QModelIndex myBottomRight = mapFromSource(bottomRight);
    emit dataChanged(myTopLeft, myBottomRight, roles);
}

void ServerListViewProxyModel::sourceModelAboutToBeReset()
{
    beginResetModel();
}

void ServerListViewProxyModel::sourceModelReset()
{
    endResetModel();
}

void ServerListViewProxyModel::sourceModelRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    if (parent.isValid()) {
        return;
    }
    if (!mSourceModel) {
        return;
    }
    const int sourceRowCount = mSourceModel->rowCount();
    if (first < 0) {
        first = 0;
    }
    if (last >= sourceRowCount) {
        last = sourceRowCount - 1;
    }
    int myFirst = mapRowFromSource(first);
    int myLast = mapRowFromSource(last);
    if (first == 0 && last == sourceRowCount - 1) {
        // all separators covered, nothing to add.
    }
    else if (last < sourceRowCount - 1) {
        // also remove separator after the actual rows
        myLast += 1;
    }
    else if (first > 0) {
        // row before the removed rows becomes new last, so remove the separator
        myFirst -= 1;
    }

    beginRemoveRows(QModelIndex(), myFirst, myLast);
}

void ServerListViewProxyModel::sourceModelRowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    if (parent.isValid()) {
        return;
    }
    if (!mSourceModel) {
        return;
    }
    if (!mSourceModel) {
        return;
    }
    if (first < 0) {
        first = 0;
    }
    const int sourceRowCount = mSourceModel->rowCount();
    if (first > sourceRowCount) {
        first = sourceRowCount;
    }
    if (last < first) {
        qCritical() << "ERROR: invalid insert parameters, first:" << first << "last:" << last;
        last = first;
    }
    int myFirst = mapRowFromSource(first);
    int myLast = mapRowFromSource(last);
    if (first == sourceRowCount) {
        // inserting at the end. no need to add separator, "last" item does not have separators.
    }
    else {
        // inserting before the end - add separator
        myLast += 1;
    }

    beginInsertRows(QModelIndex(), myFirst, myLast);
}

void ServerListViewProxyModel::sourceModelRowsInserted(const QModelIndex& parent, int first, int last)
{
    if (parent.isValid()) {
        return;
    }
    Q_UNUSED(first);
    Q_UNUSED(last);
    endInsertRows();
}

void ServerListViewProxyModel::sourceModelRowsRemoved(const QModelIndex& parent, int first, int last)
{
    if (parent.isValid()) {
        return;
    }
    Q_UNUSED(first);
    Q_UNUSED(last);
    endRemoveRows();
}


ServerListView::ServerListView(QWidget* parent)
    : QListView(parent)
{
}

ServerListView::~ServerListView()
{
}

QModelIndex ServerListView::indexAt(const QPoint& pos) const
{
    QModelIndex idx = QListView::indexAt(pos);
    if (idx.isValid() && idx.data((int)ServerListModel::Roles::ItemType).toInt() == (int)ServerListModel::ItemType::Server) {
        ServerListItemDelegate* delegate = qobject_cast<ServerListItemDelegate*>(itemDelegate(idx));
        ServerListViewProxyModel* proxyModel = qobject_cast<ServerListViewProxyModel*>(model());
        if (!delegate || !proxyModel) {
            return idx;
        }
        QRect itemRect = visualRect(idx);
        if (delegate->isEditButton(itemRect, pos)) {
            return proxyModel->makeEditButtonIndex(idx.row());
        }
        return idx;
    }
    return idx;
}

