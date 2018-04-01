#ifndef SERERLISTMODEL_H
#define SERERLISTMODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QDateTime>

#include <libopenrv/libopenrv.h>

Q_DECLARE_METATYPE(orv_communication_pixel_format_t);

class ServerListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Server
    {
        QString mHostName;
        int mPort = 5900;
        QString mEncryptedPassword;
        int mDecryptedPasswordLength = 0; // may be used by GUI to display number of stars. not saved to file.
        QString mName;
        bool mSavePassword = true;
        int mInternalServerId = 0; // invalid Id
        QDateTime mEntryCreatedDateTime;
        QDateTime mLastConnectedDateTime;
        bool mViewOnly = true;
        orv_communication_quality_profile_t mCommunicationQualityProfile = ORV_COMM_QUALITY_PROFILE_BEST;
        orv_communication_pixel_format_t mCustomPixelFormat; // only valid if mCommunicationQualityProfile is ORV_COMM_QUALITY_PROFILE_CUSTOM

        // TODO: thumbnail
    };
    enum class Roles
    {
        ItemType = Qt::UserRole + 1,
        HostName,
        Port,
        Name,
        SavePassword,
        PasswordLength,
        EncryptedPassword,
        InternalServerId,
        LastConnection,
        ViewOnly,
        CommunicationQualityProfile,
        CustomPixelFormat,
        EntryCreated,
    };
    enum class ItemType
    {
        /**
         * An item of the ServerListModel
         **/
        Server = 0,
        /**
         * A separator item, added by a proxy model (not used by ServerListModel directly)
         **/
        Separator,
    };
    static const int mServerThumbnailWidth = 60;
    static const int mServerThumbnailHeight = 60;
public:
    ServerListModel(QObject* parent = nullptr);
    virtual ~ServerListModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    int addNewServer(const QString& hostName, int port, const QString& password, const QString& name, bool savePassword, bool viewOnly, orv_communication_quality_profile_t qualityProfile, const orv_communication_pixel_format_t& customPixelFormat);
    bool updateServer(int internalServerId, const QString& hostName, int port, const QString& password, const QString& name, bool savePassword, bool viewOnly, orv_communication_quality_profile_t qualityProfile, orv_communication_pixel_format_t customPixelFormat);
    bool updateLastConnectoinForServer(int internalServerId, const QDateTime& lastConnection);
    bool deleteServer(int internalServerId);

    bool saveServerListToFile();
    bool loadServerListFromFile();

    QString decryptPassword(const QString& encryptedPassword) const;

protected:
    QString encryptPassword(const QString& plainTextPassword) const;
    void editServer(Server* s, const QString& hostName, int port, const QString& password, const QString& name, bool savePassword, bool viewOnly, orv_communication_quality_profile_t qualityProfile, orv_communication_pixel_format_t customPixelFormat) const;

private:
    int mNextServerId = 1;
    QList<Server> mServers;
};

#endif

