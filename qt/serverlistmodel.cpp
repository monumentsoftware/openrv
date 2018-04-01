#include "serverlistmodel.h"

#include <QtCore/QtDebug>
#include <QtCore/QSettings>

ServerListModel::ServerListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    if (!loadServerListFromFile()) {
        qWarning() << "Failed loading existing server list";
    }
    else {
        //qDebug() << "Loaded" << mServers.count() << "servers";
    }
}

ServerListModel::~ServerListModel()
{
}

int ServerListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return mServers.count();
}

QVariant ServerListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (index.column() != 0) {
        return QVariant();
    }
    if (index.row() < 0 || index.row() >= mServers.count()) {
        return QVariant();
    }
    const Server& s = mServers.at(index.row());
    if (role == Qt::DisplayRole) {
        if (s.mName.isEmpty()) {
            return s.mHostName;
        }
        return s.mName;
    }
    switch ((Roles)role) {
        case Roles::ItemType:
            return qVariantFromValue((int)ServerListModel::ItemType::Server);
        case Roles::HostName:
            return qVariantFromValue(s.mHostName);
        case Roles::Name:
            return qVariantFromValue(s.mName);
        case Roles::Port:
            return qVariantFromValue(s.mPort);
        case Roles::SavePassword:
            return qVariantFromValue(s.mSavePassword);
        case Roles::PasswordLength:
            return qVariantFromValue(s.mDecryptedPasswordLength);
        case Roles::EncryptedPassword:
            return qVariantFromValue(s.mEncryptedPassword);
        case Roles::InternalServerId:
            return qVariantFromValue(s.mInternalServerId);
        case Roles::LastConnection:
            return qVariantFromValue(s.mLastConnectedDateTime);
        case Roles::EntryCreated:
            return qVariantFromValue(s.mEntryCreatedDateTime);
        case Roles::ViewOnly:
            return qVariantFromValue(s.mViewOnly);
        case Roles::CommunicationQualityProfile:
            return qVariantFromValue((int)s.mCommunicationQualityProfile);
        case Roles::CustomPixelFormat:
            return qVariantFromValue(s.mCustomPixelFormat);
    }
    return QVariant();
}

int ServerListModel::addNewServer(const QString& hostName, int port, const QString& password, const QString& name, bool savePassword, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat)
{
    int internalServerId = mNextServerId;
    mNextServerId++;

    Server s;
    editServer(&s, hostName, port, password, name, savePassword, viewOnly, qualityProfile, customPixelFormat);
    s.mInternalServerId = internalServerId;
    s.mEntryCreatedDateTime = QDateTime::currentDateTime();

    beginInsertRows(QModelIndex(), mServers.count(), mServers.count());
    mServers.append(s);
    endInsertRows();

    if (!saveServerListToFile()) {
        qWarning() << "Failed to save server list to file";
    }

    return internalServerId;
}

bool ServerListModel::updateServer(int internalServerId, const QString& hostName, int port, const QString& password, const QString& name, bool savePassword, bool viewOnly, orv_communication_quality_profile_t qualityProfile, orv_communication_pixel_format_t customPixelFormat)
{
    if (internalServerId <= 0) {
        qCritical() << "ERROR: Invalid internalServerId" << internalServerId;
        return false;
    }
    for (int i = 0; i < mServers.count(); i++) {
        if (mServers.at(i).mInternalServerId == internalServerId) {
            Server& s = mServers[i];
            editServer(&s, hostName, port, password, name, savePassword, viewOnly, qualityProfile, customPixelFormat);
            if (!saveServerListToFile()) {
                qWarning() << "Failed to save server list to file";
            }
            QModelIndex modelIndex = this->index(i, 0, QModelIndex());
            emit dataChanged(modelIndex, modelIndex);
            return true;
        }
    }
    qCritical() << "ERROR: Could not find server with internalServerId" << internalServerId;
    return false;
}

void ServerListModel::editServer(Server* s, const QString& hostName, int port, const QString& password, const QString& name, bool savePassword, bool viewOnly, orv_communication_quality_profile_t qualityProfile, orv_communication_pixel_format_t customPixelFormat) const
{
    s->mHostName = hostName;
    s->mPort = port;
    if (savePassword) {
        s->mEncryptedPassword = encryptPassword(password);
        s->mDecryptedPasswordLength = password.count();
    }
    else {
        s->mEncryptedPassword = QString();
        s->mDecryptedPasswordLength = 0;
    }
    s->mName = name;
    s->mSavePassword = savePassword;
    s->mViewOnly = viewOnly;
    s->mCommunicationQualityProfile = qualityProfile;
    orv_communication_pixel_format_copy(&s->mCustomPixelFormat, &customPixelFormat);
}

bool ServerListModel::updateLastConnectoinForServer(int internalServerId, const QDateTime& lastConnection)
{
    if (internalServerId <= 0) {
        qCritical() << "ERROR: Invalid internalServerId" << internalServerId;
        return false;
    }
    for (int i = 0; i < mServers.count(); i++) {
        if (mServers.at(i).mInternalServerId == internalServerId) {
            Server& s = mServers[i];
            s.mLastConnectedDateTime = lastConnection;
            if (!saveServerListToFile()) {
                qWarning() << "Failed to save server list to file";
            }
            QModelIndex modelIndex = this->index(i, 0, QModelIndex());
            emit dataChanged(modelIndex, modelIndex);
            return true;
        }
    }
    qCritical() << "ERROR: Could not find server with internalServerId" << internalServerId;
    return false;
}

bool ServerListModel::deleteServer(int internalServerId)
{
    if (internalServerId <= 0) {
        qCritical() << "ERROR: Invalid internalServerId" << internalServerId;
        return false;
    }
    for (int i = 0; i < mServers.count(); i++) {
        if (mServers.at(i).mInternalServerId == internalServerId) {
            beginRemoveRows(QModelIndex(), i, i);
            mServers.removeAt(i);
            endRemoveRows();
            if (!saveServerListToFile()) {
                qWarning() << "Failed to save server list to file";
            }
            return true;
        }
    }
    qWarning() << "Unable to find server with internalServerId" << internalServerId << "could not delete server.";
    return false;
}

bool ServerListModel::saveServerListToFile()
{
    QSettings settings;
    settings.setValue("NextServerId", qVariantFromValue(mNextServerId));
    settings.setValue("ServerCount", qVariantFromValue(mServers.count()));
    QStringList groups = settings.childGroups();
    for (int i = 0; i < mServers.count(); i++) {
        const Server& s = mServers.at(i);
        settings.beginGroup(QString("Server_%1").arg(i));
        settings.setValue("Name", qVariantFromValue(s.mName));
        settings.setValue("HostName", qVariantFromValue(s.mHostName));
        settings.setValue("Port", qVariantFromValue(s.mPort));
        settings.setValue("SavePassword", qVariantFromValue(s.mSavePassword));
        if (s.mSavePassword) {
            settings.setValue("Password", qVariantFromValue(s.mEncryptedPassword));
        }
        else {
            settings.remove("Password");
        }
        settings.setValue("ServerId", qVariantFromValue(s.mInternalServerId));
        settings.setValue("CreatedDateTime", qVariantFromValue(s.mEntryCreatedDateTime));
        settings.setValue("LastConnectedDateTime", qVariantFromValue(s.mLastConnectedDateTime));
        settings.setValue("ViewOnly", qVariantFromValue(s.mViewOnly));
        settings.setValue("QualityProfile", QString::fromLatin1(orv_get_communication_quality_profile_string(s.mCommunicationQualityProfile)));
        settings.endGroup();
        if (s.mCommunicationQualityProfile == ORV_COMM_QUALITY_PROFILE_CUSTOM) {
            settings.beginGroup(QString("Server_%1_CustomPixelFormat").arg(i));
            settings.setValue("BitsPerPixel", qVariantFromValue((int)s.mCustomPixelFormat.mBitsPerPixel));
            settings.setValue("BigEndian", qVariantFromValue((bool)s.mCustomPixelFormat.mBigEndian));
            settings.setValue("TrueColor", qVariantFromValue((bool)s.mCustomPixelFormat.mTrueColor));
            settings.setValue("MaxRed", qVariantFromValue((int)s.mCustomPixelFormat.mColorMax[0]));
            settings.setValue("MaxGreen", qVariantFromValue((int)s.mCustomPixelFormat.mColorMax[1]));
            settings.setValue("MaxBlue", qVariantFromValue((int)s.mCustomPixelFormat.mColorMax[2]));
            settings.setValue("ShiftRed", qVariantFromValue((int)s.mCustomPixelFormat.mColorShift[0]));
            settings.setValue("ShiftGreen", qVariantFromValue((int)s.mCustomPixelFormat.mColorShift[1]));
            settings.setValue("ShiftBlue", qVariantFromValue((int)s.mCustomPixelFormat.mColorShift[2]));
            settings.endGroup();
        }
        else {
            settings.remove(QString("Server_%1_CustomPixelFormat").arg(i));
        }
    }

    for (QString group : groups) {
        if (!group.startsWith("Server_")) {
            continue;
        }
        bool ok;
        int index = group.right(group.count() - QString("Server_").count()).toInt(&ok);
        if (!ok) {
            settings.remove(group);
            continue;
        }
        if (index >= mServers.count()) {
            settings.remove(group);
        }

    }
    return true;
}

bool ServerListModel::loadServerListFromFile()
{
    QSettings settings;
    int nextServerId = settings.value("NextServerId").toInt();
    if (nextServerId > mNextServerId) {
        mNextServerId = nextServerId;
    }

    QList<Server> servers;
    int serverCount = settings.value("ServerCount").toInt();
    QSet<int> existingIds;
    for (int i = 0; i < serverCount; i++) {
        settings.beginGroup(QString("Server_%1").arg(i));
        Server s;
        s.mHostName = settings.value("HostName").toString();
        s.mName = settings.value("Name").toString();
        s.mPort = settings.value("Port").toInt();
        s.mInternalServerId = settings.value("ServerId").toInt();
        s.mSavePassword = settings.value("SavePassword").toBool();
        if (s.mSavePassword) {
            s.mEncryptedPassword = settings.value("Password").toString();
            QString password = decryptPassword(s.mEncryptedPassword);
            s.mDecryptedPasswordLength = password.count();
        }
        s.mEntryCreatedDateTime = settings.value("CreatedDateTime").toDateTime();
        s.mLastConnectedDateTime = settings.value("LastConnectedDateTime").toDateTime();
        s.mViewOnly = settings.value("ViewOnly").toBool();
        QByteArray qualityProfileString = settings.value("QualityProfile").toString().toLatin1();
        s.mCommunicationQualityProfile = orv_get_communication_quality_profile_from_string(qualityProfileString.constData(), ORV_COMM_QUALITY_PROFILE_BEST); // TODO: fallback: adaptive?
        settings.endGroup();
        if (s.mCommunicationQualityProfile == ORV_COMM_QUALITY_PROFILE_CUSTOM) {
            settings.beginGroup(QString("Server_%1_CustomPixelFormat").arg(i));
            s.mCustomPixelFormat.mBitsPerPixel = settings.value("BitsPerPixel").toInt();
            s.mCustomPixelFormat.mBigEndian = settings.value("BigEndian").toBool();
            s.mCustomPixelFormat.mTrueColor = settings.value("TrueColor").toBool();
            s.mCustomPixelFormat.mColorMax[0] = settings.value("MaxRed").toInt();
            s.mCustomPixelFormat.mColorMax[1] = settings.value("MaxGreen").toInt();
            s.mCustomPixelFormat.mColorMax[2] = settings.value("MaxBlue").toInt();
            s.mCustomPixelFormat.mColorShift[0] = settings.value("ShiftRed").toInt();
            s.mCustomPixelFormat.mColorShift[1] = settings.value("ShiftGreen").toInt();
            s.mCustomPixelFormat.mColorShift[2] = settings.value("ShiftBlue").toInt();
            settings.endGroup();
        }

        if (s.mInternalServerId <= 0) {
            qWarning() << "Invalid internal server id" << s.mInternalServerId << "for server" << s.mHostName << "in config";
            continue;
        }
        if (existingIds.contains(s.mInternalServerId)) {
            qWarning() << "Duplicated internal server id" << s.mInternalServerId << "in config";
            continue;
        }
        existingIds.insert(s.mInternalServerId);
        servers.append(s);
    }

    beginResetModel();
    mServers = servers;
    endResetModel();
    return true;
}


#warning TODO: some simple password encryption/obfuscation!
// TODO: use keychain, if on osx!
QString ServerListModel::encryptPassword(const QString& plainTextPassword) const
{
    return plainTextPassword;
}
QString ServerListModel::decryptPassword(const QString& encryptedPassword) const
{
    return encryptedPassword;
}

