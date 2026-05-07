#include "SensorEventModel.h"

#include <QDateTime>

namespace {
constexpr int kMaxSensorEvents = 80;
}

SensorEventModel::SensorEventModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SensorEventModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant SensorEventModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    return valueForRole(m_items.at(index.row()), role);
}

QHash<int, QByteArray> SensorEventModel::roleNames() const
{
    return {
        {ModelNameRole, "modelName"},
        {DeviceIdRole, "deviceId"},
        {ChannelRole, "channel"},
        {SummaryRole, "summary"},
        {TemperatureRole, "temperatureC"},
        {HasTemperatureRole, "hasTemperature"},
        {HumidityRole, "humidity"},
        {HasHumidityRole, "hasHumidity"},
        {BatteryOkRole, "batteryOk"},
        {HasBatteryRole, "hasBattery"},
        {ProtocolRole, "protocol"},
        {RawRole, "raw"},
        {BitCountRole, "bitCount"},
        {RepeatCountRole, "repeatCount"},
        {SignalLevelRole, "signalLevel"},
        {FrequencyMHzRole, "frequencyMHz"},
        {EventCountRole, "eventCount"},
        {LastSeenSecondsRole, "lastSeenSeconds"},
        {CategoryRole, "category"},
        {PressureKPaRole, "pressureKPa"},
        {PressurePSIRole, "pressurePSI"},
        {PressureBarRole, "pressureBar"},
        {LowPressureRole, "lowPressure"},
        {MeterConsumptionRole, "meterConsumption"},
        {MeterUnitRole, "meterUnit"},
        {MeterTypeNameRole, "meterTypeName"},
    };
}

QVariantMap SensorEventModel::get(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    return toMap(m_items.at(row));
}

void SensorEventModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }

    beginResetModel();
    m_items.clear();
    m_indexByKey.clear();
    endResetModel();
}

int SensorEventModel::eventCount() const
{
    return m_items.size();
}

void SensorEventModel::updateEvent(const QVariantMap &event)
{
    QString key = event.value(QStringLiteral("key")).toString().trimmed();
    if (key.isEmpty()) {
        key = event.value(QStringLiteral("modelName")).toString()
            + QStringLiteral(":")
            + event.value(QStringLiteral("raw")).toString();
    }
    if (key.isEmpty()) {
        return;
    }

    if (!m_indexByKey.contains(key)) {
        beginInsertRows(QModelIndex(), 0, 0);
        SensorEventItem item = itemFromMap(event);
        item.key = key;
        item.eventCount = 1;
        m_items.prepend(item);
        endInsertRows();
        rebuildIndex();

        while (m_items.size() > kMaxSensorEvents) {
            const int row = m_items.size() - 1;
            beginRemoveRows(QModelIndex(), row, row);
            m_items.removeAt(row);
            endRemoveRows();
            rebuildIndex();
        }
        return;
    }

    int row = m_indexByKey.value(key);
    SensorEventItem updated = itemFromMap(event, &m_items.at(row));
    updated.key = key;
    updated.eventCount = m_items.at(row).eventCount + 1;
    m_items[row] = updated;

    if (row > 0) {
        beginMoveRows(QModelIndex(), row, row, QModelIndex(), 0);
        m_items.move(row, 0);
        endMoveRows();
        rebuildIndex();
        row = 0;
    }

    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed);
}

void SensorEventModel::pruneStale(int maxAgeSeconds)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool removedAny = false;

    for (int row = m_items.size() - 1; row >= 0; --row) {
        if (m_items.at(row).lastSeenMs <= 0 || now - m_items.at(row).lastSeenMs <= maxAgeSeconds * 1000LL) {
            continue;
        }
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
        removedAny = true;
    }

    if (removedAny) {
        rebuildIndex();
    }
}

SensorEventModel::SensorEventItem SensorEventModel::itemFromMap(const QVariantMap &event,
                                                                const SensorEventItem *previous) const
{
    SensorEventItem item;
    if (previous != nullptr) {
        item = *previous;
    }

    item.modelName = event.value(QStringLiteral("modelName"), item.modelName).toString();
    item.deviceId = event.value(QStringLiteral("deviceId"), item.deviceId).toString();
    item.channel = event.value(QStringLiteral("channel"), item.channel).toString();
    item.summary = event.value(QStringLiteral("summary"), item.summary).toString();
    item.temperatureC = event.value(QStringLiteral("temperatureC"), item.temperatureC).toDouble();
    item.hasTemperature = event.value(QStringLiteral("hasTemperature"), item.hasTemperature).toBool();
    item.humidity = event.value(QStringLiteral("humidity"), item.humidity).toInt();
    item.hasHumidity = event.value(QStringLiteral("hasHumidity"), item.hasHumidity).toBool();
    item.batteryOk = event.value(QStringLiteral("batteryOk"), item.batteryOk).toBool();
    item.hasBattery = event.value(QStringLiteral("hasBattery"), item.hasBattery).toBool();
    item.protocol = event.value(QStringLiteral("protocol"), item.protocol).toString();
    item.raw = event.value(QStringLiteral("raw"), item.raw).toString();
    item.bitCount = event.value(QStringLiteral("bitCount"), item.bitCount).toInt();
    item.repeatCount = event.value(QStringLiteral("repeatCount"), item.repeatCount).toInt();
    item.signalLevel = event.value(QStringLiteral("signalLevel"), item.signalLevel).toDouble();
    item.frequencyHz = event.value(QStringLiteral("frequencyHz"), item.frequencyHz).toDouble();
    item.lastSeenMs = event.value(QStringLiteral("lastSeenMs"), QDateTime::currentMSecsSinceEpoch()).toLongLong();
    item.category = event.value(QStringLiteral("category"), item.category).toString();
    item.pressureKPa = event.value(QStringLiteral("pressureKPa"), item.pressureKPa).toDouble();
    item.pressurePSI = event.value(QStringLiteral("pressurePSI"), item.pressurePSI).toDouble();
    item.pressureBar = event.value(QStringLiteral("pressureBar"), item.pressureBar).toDouble();
    item.lowPressure = event.value(QStringLiteral("lowPressure"), item.lowPressure).toBool();
    item.meterConsumption = event.value(QStringLiteral("meterConsumption"), item.meterConsumption).toDouble();
    item.meterUnit = event.value(QStringLiteral("meterUnit"), item.meterUnit).toString();
    item.meterTypeName = event.value(QStringLiteral("meterTypeName"), item.meterTypeName).toString();
    return item;
}

QVariant SensorEventModel::valueForRole(const SensorEventItem &item, int role) const
{
    switch (role) {
    case ModelNameRole:
        return item.modelName;
    case DeviceIdRole:
        return item.deviceId;
    case ChannelRole:
        return item.channel;
    case SummaryRole:
        return item.summary;
    case TemperatureRole:
        return item.temperatureC;
    case HasTemperatureRole:
        return item.hasTemperature;
    case HumidityRole:
        return item.humidity;
    case HasHumidityRole:
        return item.hasHumidity;
    case BatteryOkRole:
        return item.batteryOk;
    case HasBatteryRole:
        return item.hasBattery;
    case ProtocolRole:
        return item.protocol;
    case RawRole:
        return item.raw;
    case BitCountRole:
        return item.bitCount;
    case RepeatCountRole:
        return item.repeatCount;
    case SignalLevelRole:
        return item.signalLevel;
    case FrequencyMHzRole:
        return item.frequencyHz / 1000000.0;
    case EventCountRole:
        return item.eventCount;
    case LastSeenSecondsRole:
        return qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - item.lastSeenMs) / 1000);
    case CategoryRole:
        return item.category;
    case PressureKPaRole:
        return item.pressureKPa;
    case PressurePSIRole:
        return item.pressurePSI;
    case PressureBarRole:
        return item.pressureBar;
    case LowPressureRole:
        return item.lowPressure;
    case MeterConsumptionRole:
        return item.meterConsumption;
    case MeterUnitRole:
        return item.meterUnit;
    case MeterTypeNameRole:
        return item.meterTypeName;
    default:
        return {};
    }
}

QVariantMap SensorEventModel::toMap(const SensorEventItem &item) const
{
    QVariantMap map;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        map.insert(QString::fromLatin1(it.value()), valueForRole(item, it.key()));
    }
    return map;
}

void SensorEventModel::rebuildIndex()
{
    m_indexByKey.clear();
    for (int row = 0; row < m_items.size(); ++row) {
        m_indexByKey.insert(m_items.at(row).key, row);
    }
}
