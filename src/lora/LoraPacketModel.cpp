#include "LoraPacketModel.h"

LoraPacketModel::LoraPacketModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LoraPacketModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_packets.size());
}

QVariant LoraPacketModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_packets.size()))
        return QVariant();

    const auto& packet = m_packets[index.row()];

    switch (role) {
    case TimestampRole:
        return packet.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
    case DataRole:
        return packet.data;
    case RssiRole:
        return packet.rssi;
    case SnrRole:
        return packet.snr;
    case FrequencyRole:
        return packet.frequency;
    case BandwidthRole:
        return packet.bandwidth;
    case SpreadingFactorRole:
        return packet.spreadingFactor;
    case CrcValidRole:
        return packet.crcValid;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LoraPacketModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TimestampRole] = "timestamp";
    roles[DataRole] = "data";
    roles[RssiRole] = "rssi";
    roles[SnrRole] = "snr";
    roles[FrequencyRole] = "frequency";
    roles[BandwidthRole] = "bandwidth";
    roles[SpreadingFactorRole] = "spreadingFactor";
    roles[CrcValidRole] = "crcValid";
    return roles;
}

void LoraPacketModel::clear()
{
    beginResetModel();
    m_packets.clear();
    endResetModel();
    emit countChanged();
}

void LoraPacketModel::addPacket(const QDateTime& timestamp, const QString& data,
                                int rssi, float snr, int frequency,
                                int bandwidth, int spreadingFactor)
{
    // 限制最大数量
    if (m_packets.size() >= MAX_PACKETS) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_packets.erase(m_packets.begin());
        endRemoveRows();
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    
    LoraPacketInfo packet;
    packet.timestamp = timestamp;
    packet.data = data;
    packet.rssi = rssi;
    packet.snr = snr;
    packet.frequency = frequency;
    packet.bandwidth = bandwidth;
    packet.spreadingFactor = spreadingFactor;
    packet.crcValid = true;  // 简化处理
    
    m_packets.push_back(packet);
    endInsertRows();
    
    emit countChanged();
}