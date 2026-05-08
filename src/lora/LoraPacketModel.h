#ifndef LORAPACKETMODEL_H
#define LORAPACKETMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <vector>

struct LoraPacketInfo {
    QDateTime timestamp;
    QString data;
    int rssi;
    float snr;
    int frequency;
    int bandwidth;
    int spreadingFactor;
    bool crcValid;
};

class LoraPacketModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        DataRole,
        RssiRole,
        SnrRole,
        FrequencyRole,
        BandwidthRole,
        SpreadingFactorRole,
        CrcValidRole
    };

    explicit LoraPacketModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void addPacket(const QDateTime& timestamp, const QString& data, 
                               int rssi, float snr, int frequency, 
                               int bandwidth, int spreadingFactor);

signals:
    void countChanged();

private:
    std::vector<LoraPacketInfo> m_packets;
    static constexpr int MAX_PACKETS = 1000;
};

#endif // LORAPACKETMODEL_H