#include "AdsbAircraftModel.h"

#include <algorithm>

AdsbAircraftModel::AdsbAircraftModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AdsbAircraftModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_items.size();
}

QVariant AdsbAircraftModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    return valueForRole(m_items.at(index.row()), role);
}

QHash<int, QByteArray> AdsbAircraftModel::roleNames() const
{
    return {
        {IcaoRole, "icao"},
        {CallsignRole, "callsign"},
        {AltitudeRole, "altitude"},
        {HasAltitudeRole, "hasAltitude"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {HasPositionRole, "hasPosition"},
        {SpeedRole, "speed"},
        {HasSpeedRole, "hasSpeed"},
        {HeadingRole, "heading"},
        {HasHeadingRole, "hasHeading"},
        {MessagesRole, "messages"},
        {LastSeenSecondsRole, "lastSeenSeconds"},
    };
}

QVariantMap AdsbAircraftModel::get(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }

    return toMap(m_items.at(row));
}

void AdsbAircraftModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }

    beginResetModel();
    m_items.clear();
    m_indexByIcao.clear();
    endResetModel();
}

int AdsbAircraftModel::aircraftCount() const
{
    return m_items.size();
}

void AdsbAircraftModel::updateAircraft(const QVariantMap &aircraft)
{
    const QString icao = aircraft.value(QStringLiteral("icao")).toString().trimmed().toUpper();
    if (icao.isEmpty()) {
        return;
    }

    const auto readItem = [&]() {
        AircraftItem item;
        item.icao = icao;
        item.callsign = aircraft.value(QStringLiteral("callsign")).toString().trimmed();
        item.altitude = aircraft.value(QStringLiteral("altitude")).toInt();
        item.hasAltitude = aircraft.value(QStringLiteral("hasAltitude")).toBool();
        item.latitude = aircraft.value(QStringLiteral("latitude")).toDouble();
        item.longitude = aircraft.value(QStringLiteral("longitude")).toDouble();
        item.hasPosition = aircraft.value(QStringLiteral("hasPosition")).toBool();
        item.speed = aircraft.value(QStringLiteral("speed")).toDouble();
        item.hasSpeed = aircraft.value(QStringLiteral("hasSpeed")).toBool();
        item.heading = aircraft.value(QStringLiteral("heading")).toDouble();
        item.hasHeading = aircraft.value(QStringLiteral("hasHeading")).toBool();
        item.messages = aircraft.value(QStringLiteral("messages")).toInt();
        item.lastSeenMs = aircraft.value(QStringLiteral("lastSeenMs")).toLongLong();
        return item;
    };

    if (!m_indexByIcao.contains(icao)) {
        const int row = m_items.size();
        beginInsertRows(QModelIndex(), row, row);
        m_indexByIcao.insert(icao, row);
        m_items.push_back(readItem());
        endInsertRows();
        return;
    }

    const int row = m_indexByIcao.value(icao);
    m_items[row] = readItem();
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed);
}

void AdsbAircraftModel::pruneStale(int maxAgeSeconds)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool removedAny = false;

    for (int row = m_items.size() - 1; row >= 0; --row) {
        const AircraftItem &item = m_items.at(row);
        if (item.lastSeenMs <= 0 || now - item.lastSeenMs <= maxAgeSeconds * 1000LL) {
            continue;
        }

        beginRemoveRows(QModelIndex(), row, row);
        m_indexByIcao.remove(item.icao);
        m_items.removeAt(row);
        endRemoveRows();
        removedAny = true;
    }

    if (!removedAny) {
        return;
    }

    m_indexByIcao.clear();
    for (int row = 0; row < m_items.size(); ++row) {
        m_indexByIcao.insert(m_items.at(row).icao, row);
    }
}

QVariant AdsbAircraftModel::valueForRole(const AircraftItem &item, int role) const
{
    switch (role) {
    case IcaoRole:
        return item.icao;
    case CallsignRole:
        return item.callsign.isEmpty() ? item.icao : item.callsign;
    case AltitudeRole:
        return item.altitude;
    case HasAltitudeRole:
        return item.hasAltitude;
    case LatitudeRole:
        return item.latitude;
    case LongitudeRole:
        return item.longitude;
    case HasPositionRole:
        return item.hasPosition;
    case SpeedRole:
        return item.speed;
    case HasSpeedRole:
        return item.hasSpeed;
    case HeadingRole:
        return item.heading;
    case HasHeadingRole:
        return item.hasHeading;
    case MessagesRole:
        return item.messages;
    case LastSeenSecondsRole:
        return qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - item.lastSeenMs) / 1000);
    default:
        return {};
    }
}

QVariantMap AdsbAircraftModel::toMap(const AircraftItem &item) const
{
    QVariantMap map;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        map.insert(QString::fromLatin1(it.value()), valueForRole(item, it.key()));
    }
    return map;
}
