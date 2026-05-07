#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>
#include <QVector>

class SensorEventModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        ModelNameRole = Qt::UserRole + 1,
        DeviceIdRole,
        ChannelRole,
        SummaryRole,
        TemperatureRole,
        HasTemperatureRole,
        HumidityRole,
        HasHumidityRole,
        BatteryOkRole,
        HasBatteryRole,
        ProtocolRole,
        RawRole,
        BitCountRole,
        RepeatCountRole,
        SignalLevelRole,
        FrequencyMHzRole,
        EventCountRole,
        LastSeenSecondsRole,
        // Scene / category
        CategoryRole,
        // TPMS fields
        PressureKPaRole,
        PressurePSIRole,
        PressureBarRole,
        LowPressureRole,
        // Utility meter fields
        MeterConsumptionRole,
        MeterUnitRole,
        MeterTypeNameRole
    };

    explicit SensorEventModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void clear();

    int eventCount() const;
    void updateEvent(const QVariantMap &event);
    void pruneStale(int maxAgeSeconds);

private:
    struct SensorEventItem {
        QString key;
        QString modelName;
        QString deviceId;
        QString channel;
        QString summary;
        double temperatureC = 0.0;
        bool hasTemperature = false;
        int humidity = 0;
        bool hasHumidity = false;
        bool batteryOk = false;
        bool hasBattery = false;
        QString protocol;
        QString raw;
        int bitCount = 0;
        int repeatCount = 0;
        double signalLevel = 0.0;
        double frequencyHz = 0.0;
        int eventCount = 0;
        qint64 lastSeenMs = 0;
        // Scene / category
        QString category;
        // TPMS
        double pressureKPa = 0.0;
        double pressurePSI = 0.0;
        double pressureBar = 0.0;
        bool lowPressure = false;
        // Utility meter
        double meterConsumption = 0.0;
        QString meterUnit;
        QString meterTypeName;
    };

    SensorEventItem itemFromMap(const QVariantMap &event, const SensorEventItem *previous = nullptr) const;
    QVariant valueForRole(const SensorEventItem &item, int role) const;
    QVariantMap toMap(const SensorEventItem &item) const;
    void rebuildIndex();

    QVector<SensorEventItem> m_items;
    QHash<QString, int> m_indexByKey;
};
