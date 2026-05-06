#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QVector>

class AdsbAircraftModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IcaoRole = Qt::UserRole + 1,
        CallsignRole,
        AltitudeRole,
        HasAltitudeRole,
        LatitudeRole,
        LongitudeRole,
        HasPositionRole,
        SpeedRole,
        HasSpeedRole,
        HeadingRole,
        HasHeadingRole,
        MessagesRole,
        LastSeenSecondsRole
    };

    explicit AdsbAircraftModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void clear();

    int aircraftCount() const;
    void updateAircraft(const QVariantMap &aircraft);
    void pruneStale(int maxAgeSeconds);

private:
    struct AircraftItem {
        QString icao;
        QString callsign;
        int altitude = 0;
        bool hasAltitude = false;
        double latitude = 0.0;
        double longitude = 0.0;
        bool hasPosition = false;
        double speed = 0.0;
        bool hasSpeed = false;
        double heading = 0.0;
        bool hasHeading = false;
        int messages = 0;
        qint64 lastSeenMs = 0;
    };

    QVariant valueForRole(const AircraftItem &item, int role) const;
    QVariantMap toMap(const AircraftItem &item) const;

    QVector<AircraftItem> m_items;
    QHash<QString, int> m_indexByIcao;
};
