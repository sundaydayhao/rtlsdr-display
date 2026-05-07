#include "SensorDecoder.h"
#include "TpmsDecoder.h"

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
struct BitRow
{
    QVector<int> bits;
    int repeats = 1;
    QString raw;
};

QVariantMap eventBase(const QString &modelName,
                      const QString &protocol,
                      const BitRow &row,
                      double frequencyHz,
                      double signalLevel);
void finishEvent(QVariantMap *event);

int signExtend(int value, int bits)
{
    const int sign = 1 << (bits - 1);
    return (value ^ sign) - sign;
}

QVector<int> bytesForBits(const QVector<int> &bits)
{
    QVector<int> bytes((bits.size() + 7) / 8, 0);
    for (int index = 0; index < bits.size(); ++index) {
        if (bits.at(index) == 0) {
            continue;
        }
        bytes[index / 8] |= 1 << (7 - (index % 8));
    }
    return bytes;
}

QVector<int> invertedBits(const QVector<int> &bits)
{
    QVector<int> inverted;
    inverted.reserve(bits.size());
    for (int bit : bits) {
        inverted.append(bit == 0 ? 1 : 0);
    }
    return inverted;
}

int bitsToInt(const QVector<int> &bits, int offset, int count)
{
    int value = 0;
    for (int index = 0; index < count; ++index) {
        value <<= 1;
        if (offset + index < bits.size() && bits.at(offset + index) != 0) {
            value |= 1;
        }
    }
    return value;
}

uint8_t reverseByte(int value)
{
    uint8_t byte = static_cast<uint8_t>(value & 0xff);
    byte = static_cast<uint8_t>((byte & 0xf0) >> 4 | (byte & 0x0f) << 4);
    byte = static_cast<uint8_t>((byte & 0xcc) >> 2 | (byte & 0x33) << 2);
    byte = static_cast<uint8_t>((byte & 0xaa) >> 1 | (byte & 0x55) << 1);
    return byte;
}

QString hexValue(uint64_t value, int width)
{
    return QStringLiteral("0x%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

QString rawForBits(const QVector<int> &bits)
{
    QString raw;
    raw.reserve((bits.size() + 3) / 4);
    for (int offset = 0; offset < bits.size(); offset += 4) {
        int nibble = 0;
        for (int bit = 0; bit < 4; ++bit) {
            nibble <<= 1;
            if (offset + bit < bits.size()) {
                nibble |= bits.at(offset + bit) ? 1 : 0;
            }
        }
        raw.append(QString::number(nibble, 16).toUpper());
    }
    return raw;
}

void addRow(QList<BitRow> *rows, const QVector<int> &bits, int repeats = 1)
{
    if (bits.size() < 8) {
        return;
    }

    BitRow next;
    next.bits = bits;
    next.raw = rawForBits(bits);

    for (BitRow &row : *rows) {
        if (row.bits.size() == next.bits.size() && row.raw == next.raw) {
            row.repeats += repeats;
            return;
        }
    }

    next.repeats = repeats;
    rows->append(next);
}

QList<BitRow> ppmRows(const std::vector<SensorPulse> &pulses,
                      int shortGapUs,
                      int longGapUs,
                      int rowGapUs)
{
    QList<BitRow> rows;
    QVector<int> current;
    const int thresholdUs = (shortGapUs + longGapUs) / 2;

    for (size_t index = 0; index + 1 < pulses.size(); ++index) {
        const SensorPulse &pulse = pulses[index];
        const SensorPulse &gap = pulses[index + 1];
        if (!pulse.high || gap.high) {
            continue;
        }

        if (pulse.durationUs < 80 || pulse.durationUs > 2200) {
            continue;
        }

        if (gap.durationUs >= rowGapUs) {
            addRow(&rows, current);
            current.clear();
            continue;
        }

        if (gap.durationUs < shortGapUs / 2 || gap.durationUs > longGapUs + longGapUs / 2) {
            addRow(&rows, current);
            current.clear();
            continue;
        }

        current.append(gap.durationUs < thresholdUs ? 0 : 1);
    }

    addRow(&rows, current);
    return rows;
}

QList<BitRow> pwmRows(const std::vector<SensorPulse> &pulses,
                      int shortPulseUs,
                      int longPulseUs,
                      int rowGapUs)
{
    QList<BitRow> rows;
    QVector<int> current;
    const int thresholdUs = (shortPulseUs + longPulseUs) / 2;

    for (size_t index = 0; index + 1 < pulses.size(); ++index) {
        const SensorPulse &pulse = pulses[index];
        const SensorPulse &gap = pulses[index + 1];
        if (!pulse.high || gap.high) {
            continue;
        }

        if (pulse.durationUs > shortPulseUs / 3 && pulse.durationUs < longPulseUs * 2) {
            current.append(pulse.durationUs < thresholdUs ? 1 : 0);
        }

        if (gap.durationUs >= rowGapUs) {
            addRow(&rows, current);
            current.clear();
        }
    }

    addRow(&rows, current);
    return rows;
}

const BitRow *findRow(const QList<BitRow> &rows, int expectedBits, int minRepeats)
{
    const BitRow *best = nullptr;
    for (const BitRow &row : rows) {
        if (row.bits.size() < expectedBits || row.bits.size() > expectedBits + 1) {
            continue;
        }
        if (row.repeats < minRepeats) {
            continue;
        }
        if (best == nullptr || row.repeats > best->repeats) {
            best = &row;
        }
    }
    return best;
}

QList<BitRow> fixedWidthRows(const QList<BitRow> &rows, int bitCount)
{
    QList<BitRow> windows;
    for (const BitRow &row : rows) {
        if (row.bits.size() < bitCount) {
            continue;
        }

        if (row.bits.size() == bitCount) {
            BitRow exact = row;
            exact.raw = rawForBits(exact.bits);
            addRow(&windows, exact.bits, exact.repeats);
            continue;
        }

        for (int offset = 0; offset + bitCount <= row.bits.size(); ++offset) {
            addRow(&windows, row.bits.mid(offset, bitCount), row.repeats);
        }
    }
    return windows;
}

bool appendBestRepeatedWindow(const QList<BitRow> &rows,
                              const QList<int> &bitCounts,
                              QList<QVariantMap> *events,
                              const QString &protocol,
                              double frequencyHz,
                              double signalLevel)
{
    BitRow best;
    bool hasBest = false;

    for (int bitCount : bitCounts) {
        const QList<BitRow> windows = fixedWidthRows(rows, bitCount);
        for (const BitRow &window : windows) {
            if (!hasBest
                    || window.repeats > best.repeats
                    || (window.repeats == best.repeats && window.bits.size() > best.bits.size())) {
                best = window;
                hasBest = true;
            }
        }
    }

    if (!hasBest) {
        return false;
    }

    QVariantMap event = eventBase(QStringLiteral("Generic-CarKey-RKE"),
                                  protocol,
                                  best,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString());
    event.insert(QStringLiteral("channel"), QString());
    event.insert(QStringLiteral("hasBattery"), false);
    event.insert(QStringLiteral("batteryOk"), false);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("RKE-like fixed %1 bit / raw %2")
                     .arg(best.bits.size())
                     .arg(best.raw));
    events->append(event);
    return true;
}

QVariantMap eventBase(const QString &modelName,
                      const QString &protocol,
                      const BitRow &row,
                      double frequencyHz,
                      double signalLevel)
{
    QVariantMap event;
    event.insert(QStringLiteral("modelName"), modelName);
    event.insert(QStringLiteral("protocol"), protocol);
    event.insert(QStringLiteral("raw"), row.raw);
    event.insert(QStringLiteral("bitCount"), qMin(row.bits.size(), row.raw.size() * 4));
    event.insert(QStringLiteral("repeatCount"), row.repeats);
    event.insert(QStringLiteral("frequencyHz"), frequencyHz);
    event.insert(QStringLiteral("signalLevel"), signalLevel);
    event.insert(QStringLiteral("lastSeenMs"), QDateTime::currentMSecsSinceEpoch());
    return event;
}

void finishEvent(QVariantMap *event)
{
    QStringList summary;
    if (event->value(QStringLiteral("deviceId")).toString().length() > 0) {
        summary << QStringLiteral("ID %1").arg(event->value(QStringLiteral("deviceId")).toString());
    }
    if (event->value(QStringLiteral("hasTemperature")).toBool()) {
        summary << QStringLiteral("%1 C").arg(event->value(QStringLiteral("temperatureC")).toDouble(), 0, 'f', 1);
    }
    if (event->value(QStringLiteral("hasHumidity")).toBool()) {
        summary << QStringLiteral("%1%").arg(event->value(QStringLiteral("humidity")).toInt());
    }
    if (event->value(QStringLiteral("hasBattery")).toBool()) {
        summary << (event->value(QStringLiteral("batteryOk")).toBool()
                        ? QStringLiteral("电池正常")
                        : QStringLiteral("电池低"));
    }
    if (summary.isEmpty()) {
        summary << event->value(QStringLiteral("raw")).toString();
    }

    const QString modelName = event->value(QStringLiteral("modelName")).toString();
    const QString deviceId = event->value(QStringLiteral("deviceId")).toString();
    const QString channel = event->value(QStringLiteral("channel")).toString();
    const QString raw = event->value(QStringLiteral("raw")).toString();
    const QString key = modelName + QStringLiteral(":")
        + (deviceId.isEmpty() ? raw : deviceId + QStringLiteral(":") + channel);

    event->insert(QStringLiteral("key"), key);
    event->insert(QStringLiteral("summary"), summary.join(QStringLiteral(" / ")));
}

bool plausibleTemperature(double temperatureC)
{
    return temperatureC > -80.0 && temperatureC < 90.0;
}

bool appendNexus(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    const BitRow *row = findRow(rows, 36, 3);
    if (row == nullptr) {
        return false;
    }

    QVector<int> bits = row->bits.mid(0, 36);
    const QVector<int> b = bytesForBits(bits);
    if (b.size() < 5) {
        return false;
    }
    if ((b[3] & 0xf0) != 0xf0 || (b[1] & 0x30) == 0x30) {
        return false;
    }
    if ((b[0] == 0x00 && b[2] == 0x00 && b[3] == 0x00)
            || (b[0] == 0xff && b[2] == 0xff && b[3] == 0xff)) {
        return false;
    }

    const int temperatureRaw = signExtend(((b[1] & 0x0f) << 8) | b[2], 12);
    const double temperatureC = temperatureRaw * 0.1;
    const int humidity = ((b[3] & 0x0f) << 4) | ((b[4] & 0xf0) >> 4);
    if (!plausibleTemperature(temperatureC) || humidity > 100) {
        return false;
    }

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits);
    QVariantMap event = eventBase(humidity == 0 ? QStringLiteral("Nexus-T") : QStringLiteral("Nexus-TH"),
                                  QStringLiteral("OOK PPM 1/2 ms"),
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString::number(b[0]));
    event.insert(QStringLiteral("channel"), QString::number(((b[1] & 0x30) >> 4) + 1));
    event.insert(QStringLiteral("hasBattery"), true);
    event.insert(QStringLiteral("batteryOk"), (b[1] & 0x80) != 0);
    event.insert(QStringLiteral("hasTemperature"), true);
    event.insert(QStringLiteral("temperatureC"), temperatureC);
    event.insert(QStringLiteral("hasHumidity"), humidity != 0);
    event.insert(QStringLiteral("humidity"), humidity);
    event.insert(QStringLiteral("testMode"), (b[1] & 0x40) != 0);
    finishEvent(&event);
    events->append(event);
    return true;
}

bool appendBaldr(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    const BitRow *row = findRow(rows, 64, 3);
    if (row == nullptr) {
        return false;
    }

    QVector<int> bits = row->bits.mid(0, 64);
    const QVector<int> b = bytesForBits(bits);
    if (b.size() < 8) {
        return false;
    }
    if ((b[1] & 0x40) != 0x00 || (b[3] & 0xf0) != 0xf0
            || (b[4] & 0x0f) != 0x00 || b[5] != 0x00 || (b[6] & 0xf7) != 0x00) {
        return false;
    }

    const int temperatureRaw = signExtend(((b[1] & 0x0f) << 8) | b[2], 12);
    const double temperatureC = temperatureRaw * 0.1;
    const int humidity = ((b[3] & 0x0f) << 4) | ((b[4] & 0xf0) >> 4);
    if (!plausibleTemperature(temperatureC) || humidity > 100) {
        return false;
    }

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits);
    QVariantMap event = eventBase(QStringLiteral("Baldr-E0666TH"),
                                  QStringLiteral("OOK PPM 1/2 ms"),
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString::number((b[0] << 8) | b[7]));
    event.insert(QStringLiteral("channel"), QString::number(((b[1] & 0x30) >> 4) + 1));
    event.insert(QStringLiteral("hasBattery"), true);
    event.insert(QStringLiteral("batteryOk"), (b[1] & 0x80) != 0);
    event.insert(QStringLiteral("hasTemperature"), true);
    event.insert(QStringLiteral("temperatureC"), temperatureC);
    event.insert(QStringLiteral("hasHumidity"), true);
    event.insert(QStringLiteral("humidity"), humidity);
    event.insert(QStringLiteral("startup"), (b[6] & 0x08) != 0);
    finishEvent(&event);
    events->append(event);
    return true;
}

bool appendAcurite609(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    for (const BitRow &row : rows) {
        if (row.bits.size() != 40) {
            continue;
        }

        const QVector<int> b = bytesForBits(row.bits);
        if (b.size() < 5) {
            continue;
        }
        const int checksum = (b[0] + b[1] + b[2] + b[3]) & 0xff;
        if (checksum == 0 || checksum != b[4]) {
            continue;
        }
        if (b[3] > 100) {
            continue;
        }

        const int temperatureRaw = signExtend(((b[1] & 0x0f) << 8) | b[2], 12);
        const double temperatureC = temperatureRaw * 0.1;
        if (!plausibleTemperature(temperatureC)) {
            continue;
        }

        QVariantMap event = eventBase(QStringLiteral("Acurite-609TXC"),
                                      QStringLiteral("OOK PPM 1/2 ms"),
                                      row,
                                      frequencyHz,
                                      signalLevel);
        const int status = (b[1] & 0xf0) >> 4;
        event.insert(QStringLiteral("deviceId"), QString::number(b[0]));
        event.insert(QStringLiteral("channel"), QString());
        event.insert(QStringLiteral("hasBattery"), true);
        event.insert(QStringLiteral("batteryOk"), (status & 0x08) == 0);
        event.insert(QStringLiteral("hasTemperature"), true);
        event.insert(QStringLiteral("temperatureC"), temperatureC);
        event.insert(QStringLiteral("hasHumidity"), true);
        event.insert(QStringLiteral("humidity"), b[3]);
        event.insert(QStringLiteral("status"), status);
        finishEvent(&event);
        events->append(event);
        return true;
    }

    return false;
}

bool appendThermoProTx2(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    const BitRow *row = findRow(rows, 36, 4);
    if (row == nullptr) {
        return false;
    }

    QVector<int> bits = row->bits.mid(0, 36);
    const QVector<int> b = bytesForBits(bits);
    if (b.size() < 5) {
        return false;
    }

    const int type = b[0] >> 4;
    if (type != 0x09 && type != 0x05) {
        return false;
    }

    const int temperatureRaw = signExtend((b[2] << 4) | ((b[3] & 0xf0) >> 4), 12);
    const double temperatureC = temperatureRaw * 0.1;
    const int humidity = ((b[3] & 0x0f) << 4) | ((b[4] & 0xf0) >> 4);
    if (!plausibleTemperature(temperatureC) || (humidity != 0xcc && humidity > 100)) {
        return false;
    }

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits);
    QVariantMap event = eventBase(QStringLiteral("ThermoPro-TX2"),
                                  QStringLiteral("OOK PPM 2/4 ms"),
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString::number(((b[0] & 0x0f) << 4) | ((b[1] & 0xf0) >> 4)));
    event.insert(QStringLiteral("channel"), QString::number((b[1] & 0x03) + 1));
    event.insert(QStringLiteral("hasBattery"), true);
    event.insert(QStringLiteral("batteryOk"), (b[1] & 0x08) == 0);
    event.insert(QStringLiteral("hasTemperature"), true);
    event.insert(QStringLiteral("temperatureC"), temperatureC);
    event.insert(QStringLiteral("hasHumidity"), humidity != 0xcc);
    event.insert(QStringLiteral("humidity"), humidity);
    event.insert(QStringLiteral("button"), (b[1] & 0x04) != 0);
    event.insert(QStringLiteral("subtype"), type);
    finishEvent(&event);
    events->append(event);
    return true;
}

QString buttonSummary(int button)
{
    if (button <= 0) {
        return QStringLiteral("button 0");
    }
    QStringList parts;
    if (button & 0x01) {
        parts << QStringLiteral("S0");
    }
    if (button & 0x02) {
        parts << QStringLiteral("S1");
    }
    if (button & 0x04) {
        parts << QStringLiteral("S2");
    }
    if (button & 0x08) {
        parts << QStringLiteral("S3");
    }
    return parts.isEmpty() ? QStringLiteral("button %1").arg(button) : parts.join(QStringLiteral("+"));
}

bool appendHcs200(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    const BitRow *row = findRow(rows, 66, 1);
    if (row == nullptr) {
        return false;
    }
    if (row->bits.size() != 66) {
        return false;
    }

    QVector<int> bits = row->bits.mid(0, 66);
    const QVector<int> b = bytesForBits(bits);
    if (b.size() < 9) {
        return false;
    }
    if (b[1] == 0xff && b[2] == 0xff && b[3] == 0xff && b[4] == 0xff
            && b[5] == 0xff && b[6] == 0xff && b[7] == 0xff) {
        return false;
    }

    const uint32_t encrypted = (static_cast<uint32_t>(reverseByte(b[3])) << 24)
        | (static_cast<uint32_t>(reverseByte(b[2])) << 16)
        | (static_cast<uint32_t>(reverseByte(b[1])) << 8)
        | reverseByte(b[0]);
    const uint32_t serial = (static_cast<uint32_t>(reverseByte(b[7] & 0xf0)) << 24)
        | (static_cast<uint32_t>(reverseByte(b[6])) << 16)
        | (static_cast<uint32_t>(reverseByte(b[5])) << 8)
        | reverseByte(b[4]);
    if (serial == 0 || encrypted == 0xffffffffU) {
        return false;
    }

    const int buttonBits = b[7] & 0x0f;
    const int button = (buttonBits & 0x08)
        | ((buttonBits & 0x01) << 2)
        | (buttonBits & 0x02)
        | ((buttonBits & 0x04) >> 2);
    const bool learn = buttonBits == 0x0f;
    const bool batteryLow = (b[8] & 0x80) != 0;
    const bool repeat = (b[8] & 0x40) != 0;

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits);
    QVariantMap event = eventBase(QStringLiteral("Microchip-HCS200"),
                                  QStringLiteral("OOK PWM KeeLoq HCS200/HCS300"),
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), hexValue(serial, 7));
    event.insert(QStringLiteral("channel"), QString::number(button));
    event.insert(QStringLiteral("hasBattery"), true);
    event.insert(QStringLiteral("batteryOk"), !batteryLow);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    event.insert(QStringLiteral("button"), button);
    event.insert(QStringLiteral("learn"), learn);
    event.insert(QStringLiteral("repeat"), repeat);
    event.insert(QStringLiteral("encrypted"), QStringLiteral("%1").arg(encrypted, 8, 16, QLatin1Char('0')).toUpper());
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("ID %1 / %2 / rolling %3")
                     .arg(hexValue(serial, 7),
                          buttonSummary(button),
                          QStringLiteral("0x%1").arg(encrypted, 8, 16, QLatin1Char('0')).toUpper()));
    events->append(event);
    return true;
}

bool hcs361CrcOk(const QVector<int> &bytes)
{
    if (bytes.size() < 9) {
        return false;
    }

    int crc = 0;
    int crcBatteryLow = 0;
    const int actualCrc = (bytes[8] >> 5) & 0x03;

    for (int index = 0; index < 65; ++index) {
        const int bit = (bytes[index / 8] >> (7 - (index % 8))) & 0x01;
        const int crcBit = ((crc >> 1) ^ bit) & 0x01;
        if (index == 64) {
            const int flippedBit = bit == 0 ? 1 : 0;
            const int crcBitBatteryLow = ((crc >> 1) ^ flippedBit) & 0x01;
            crcBatteryLow = crcBitBatteryLow | (((crcBitBatteryLow ^ crc) << 1) & 0x02);
        }
        crc = crcBit | (((crcBit ^ crc) << 1) & 0x02);
    }

    return actualCrc == crc || actualCrc == crcBatteryLow;
}

bool appendHcs361(const QList<BitRow> &rows,
                  const QString &protocol,
                  QList<QVariantMap> *events,
                  double frequencyHz,
                  double signalLevel)
{
    const BitRow *row = findRow(rows, 67, 1);
    if (row == nullptr) {
        return false;
    }
    if (row->bits.size() != 67) {
        return false;
    }

    QVector<int> bits = row->bits.mid(0, 67);
    const QVector<int> b = bytesForBits(bits);
    if (b.size() < 9 || !hcs361CrcOk(b)) {
        return false;
    }
    if (b[1] == 0xff && b[2] == 0xff && b[3] == 0xff && b[4] == 0xff
            && b[5] == 0xff && b[6] == 0xff && b[7] == 0xff) {
        return false;
    }

    const uint32_t encrypted = (static_cast<uint32_t>(reverseByte(b[3])) << 24)
        | (static_cast<uint32_t>(reverseByte(b[2])) << 16)
        | (static_cast<uint32_t>(reverseByte(b[1])) << 8)
        | reverseByte(b[0]);
    const uint32_t serial = (static_cast<uint32_t>(reverseByte(b[7] & 0xf0)) << 24)
        | (static_cast<uint32_t>(reverseByte(b[6])) << 16)
        | (static_cast<uint32_t>(reverseByte(b[5])) << 8)
        | reverseByte(b[4]);
    const int buttonBits = b[7] & 0x0f;
    const int button = (buttonBits & 0x08)
        | ((buttonBits & 0x01) << 2)
        | (buttonBits & 0x02)
        | ((buttonBits & 0x04) >> 2);
    const bool batteryOk = (b[8] & 0x80) != 0;

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits);
    QVariantMap event = eventBase(QStringLiteral("Microchip-HCS361"),
                                  protocol,
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), hexValue(serial, 8));
    event.insert(QStringLiteral("channel"), QString::number(button));
    event.insert(QStringLiteral("hasBattery"), true);
    event.insert(QStringLiteral("batteryOk"), batteryOk);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    event.insert(QStringLiteral("button"), button);
    event.insert(QStringLiteral("encrypted"), QStringLiteral("%1").arg(encrypted, 8, 16, QLatin1Char('0')).toUpper());
    event.insert(QStringLiteral("mic"), QStringLiteral("CRC"));
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("ID %1 / %2 / rolling %3 / CRC")
                     .arg(hexValue(serial, 8),
                          buttonSummary(button),
                          QStringLiteral("0x%1").arg(encrypted, 8, 16, QLatin1Char('0')).toUpper()));
    events->append(event);
    return true;
}

QString chryslerButtonText(int button)
{
    QStringList parts;
    if (button & 0x01) {
        parts << QStringLiteral("unlock");
    }
    if (button & 0x02) {
        parts << QStringLiteral("lock");
    }
    if (button & 0x04) {
        parts << QStringLiteral("panic");
    }
    return parts.isEmpty() ? QString() : parts.join(QStringLiteral("+"));
}

bool appendChryslerRemote(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    for (const BitRow &row : rows) {
        if (row.bits.size() < 48) {
            continue;
        }

        const int offset = row.bits.size() >= 49 ? row.bits.size() - 49 : row.bits.size() - 48;
        QVector<int> bits = invertedBits(row.bits.mid(offset, 48));
        QVector<int> bytes = bytesForBits(bits);
        if (bytes.size() < 6) {
            continue;
        }
        for (int &byte : bytes) {
            byte = reverseByte(byte);
        }

        int checksum = 0;
        for (int index = 0; index < 5; ++index) {
            checksum += bytes.at(index);
        }
        if ((checksum & 0xff) != bytes.at(5) || checksum == 0 || checksum == 0xff * 5) {
            continue;
        }

        const uint32_t id = (static_cast<uint32_t>(bytes[0]) << 24)
            | (static_cast<uint32_t>(bytes[1]) << 16)
            | (static_cast<uint32_t>(bytes[2]) << 8)
            | static_cast<uint32_t>(bytes[3]);
        const int button = bytes[4] >> 4;
        const bool multiPress = (bytes[4] & 0x04) != 0;
        const QString buttonText = chryslerButtonText(button);
        const int buttonCount = ((button & 0x01) ? 1 : 0) + ((button & 0x02) ? 1 : 0) + ((button & 0x04) ? 1 : 0);
        if (id == 0 || buttonText.isEmpty() || (buttonCount > 1 && !multiPress) || (buttonCount == 1 && multiPress)) {
            continue;
        }

        BitRow clipped = row;
        clipped.bits = bits;
        clipped.raw = rawForBits(bits);
        QVariantMap event = eventBase(QStringLiteral("Chrysler-CarRemote"),
                                      QStringLiteral("OOK PWM fixed-code RKE"),
                                      clipped,
                                      frequencyHz,
                                      signalLevel);
        event.insert(QStringLiteral("deviceId"), hexValue(id, 8));
        event.insert(QStringLiteral("channel"), QString::number(button));
        event.insert(QStringLiteral("hasBattery"), false);
        event.insert(QStringLiteral("batteryOk"), false);
        event.insert(QStringLiteral("hasTemperature"), false);
        event.insert(QStringLiteral("temperatureC"), 0.0);
        event.insert(QStringLiteral("hasHumidity"), false);
        event.insert(QStringLiteral("humidity"), 0);
        event.insert(QStringLiteral("button"), button);
        event.insert(QStringLiteral("state"), buttonText);
        event.insert(QStringLiteral("mic"), QStringLiteral("CHECKSUM"));
        finishEvent(&event);
        event.insert(QStringLiteral("summary"),
                     QStringLiteral("ID %1 / %2 / checksum").arg(hexValue(id, 8), buttonText));
        events->append(event);
        return true;
    }

    return false;
}

QString audiovoxButtonText(int flags)
{
    QStringList parts;
    if (flags & 0x01) {
        parts << QStringLiteral("lock");
    }
    if (flags & 0x04) {
        parts << QStringLiteral("unlock");
    }
    if (flags & 0x10) {
        parts << QStringLiteral("option");
    }
    if (flags & 0x40) {
        parts << QStringLiteral("trunk");
    }
    return parts.join(QStringLiteral("+"));
}

bool appendAudiovoxRemote(const QList<BitRow> &rows, QList<QVariantMap> *events, double frequencyHz, double signalLevel)
{
    const BitRow *row = findRow(rows, 25, 1);
    if (row == nullptr) {
        return false;
    }

    QVector<int> bits = invertedBits(row->bits.mid(0, 25));
    QVector<int> bytes = bytesForBits(bits);
    if (bytes.size() < 3) {
        return false;
    }

    const int id = (bytes[0] << 8) | bytes[1];
    const int flags = bytes[2];
    const QString buttonText = audiovoxButtonText(flags);
    if (id == 0 || id == 0xffff || buttonText.isEmpty()) {
        return false;
    }

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits);
    QVariantMap event = eventBase(QStringLiteral("Audiovox-PROOE3B"),
                                  QStringLiteral("OOK PWM fixed-code RKE"),
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), hexValue(id, 4));
    event.insert(QStringLiteral("channel"), QString::number(flags));
    event.insert(QStringLiteral("hasBattery"), false);
    event.insert(QStringLiteral("batteryOk"), false);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    event.insert(QStringLiteral("button"), flags);
    event.insert(QStringLiteral("state"), buttonText);
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("ID %1 / %2").arg(hexValue(id, 4), buttonText));
    events->append(event);
    return true;
}

bool appendGenericRkeRaw(const QList<BitRow> &rows,
                         const QString &protocol,
                         QList<QVariantMap> *events,
                         double frequencyHz,
                         double signalLevel)
{
    const BitRow *best = nullptr;
    for (const BitRow &row : rows) {
        if (row.bits.size() < 32 || row.bits.size() > 80) {
            continue;
        }
        if (best == nullptr || row.repeats > best->repeats || row.bits.size() > best->bits.size()) {
            best = &row;
        }
    }
    if (best == nullptr) {
        return false;
    }

    BitRow clipped = *best;
    clipped.raw = rawForBits(clipped.bits);
    QVariantMap event = eventBase(QStringLiteral("Generic-CarKey-RKE"),
                                  protocol,
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString());
    event.insert(QStringLiteral("channel"), QString());
    event.insert(QStringLiteral("hasBattery"), false);
    event.insert(QStringLiteral("batteryOk"), false);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("RKE-like %1 bit / raw %2").arg(best->bits.size()).arg(best->raw));
    events->append(event);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Toyota / Subaru RKE 遥控钥匙
// 支持多种车型：短脉冲 300–450 µs，长脉冲 600–840 µs
// 帧长 40 / 48 / 56 bit（含滚动码，raw 输出）
// 参考：rtl_433 Toyota-COROLLA14 decoder
// ─────────────────────────────────────────────────────────────────────────────
bool appendToyotaRemote(const QList<BitRow> &rows,
                        int expectedBits,
                        QList<QVariantMap> *events,
                        double frequencyHz,
                        double signalLevel)
{
    const BitRow *best = nullptr;
    for (const BitRow &row : rows) {
        if (row.bits.size() != expectedBits) {
            continue;
        }
        // 排除全 0 / 全 1 噪声帧
        const QVector<int> b = bytesForBits(row.bits);
        bool allSame = true;
        for (int i = 1; i < b.size(); ++i) {
            if (b[i] != b[0]) {
                allSame = false;
                break;
            }
        }
        if (allSame && (b[0] == 0x00 || b[0] == 0xff)) {
            continue;
        }
        if (best == nullptr || row.repeats > best->repeats) {
            best = &row;
        }
    }
    if (best == nullptr) {
        return false;
    }

    QVariantMap event = eventBase(QStringLiteral("Toyota-CarRemote"),
                                  QStringLiteral("OOK PWM Toyota/Subaru RKE"),
                                  *best,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString());
    event.insert(QStringLiteral("channel"), QString());
    event.insert(QStringLiteral("hasBattery"), false);
    event.insert(QStringLiteral("batteryOk"), false);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("丰田/速霸陆 %1 bit 滚动码 / raw %2")
                     .arg(expectedBits)
                     .arg(best->raw));
    events->append(event);
    return true;
}

QString securityCommandText(int command)
{
    switch (command) {
    case 0x0:
        return QStringLiteral("test");
    case 0x1:
        return QStringLiteral("disarm");
    case 0x2:
        return QStringLiteral("alarm");
    case 0x3:
        return QStringLiteral("tamper");
    case 0x4:
        return QStringLiteral("home");
    case 0x5:
        return QStringLiteral("on/water");
    case 0x6:
        return QStringLiteral("home-zone");
    case 0x7:
        return QStringLiteral("close/normal");
    case 0x8:
        return QStringLiteral("arm");
    case 0xa:
        return QStringLiteral("motion");
    case 0xb:
        return QStringLiteral("tamper/24h");
    case 0xc:
        return QStringLiteral("closing");
    case 0xd:
        return QStringLiteral("low-battery");
    case 0xe:
        return QStringLiteral("open");
    case 0xf:
        return QStringLiteral("battery");
    default:
        return QStringLiteral("cmd-%1").arg(command, 0, 16).toUpper();
    }
}

bool appendSecurityPwm(const QList<BitRow> &rows,
                       const QString &modelName,
                       const QString &protocol,
                       int minRepeats,
                       QList<QVariantMap> *events,
                       double frequencyHz,
                       double signalLevel)
{
    const BitRow *row = findRow(rows, 25, minRepeats);
    if (row == nullptr) {
        return false;
    }

    QVector<int> bits = invertedBits(row->bits.mid(0, 25));
    if (bits.size() != 25 || bits.at(24) == 0) {
        return false;
    }

    const int id = bitsToInt(bits, 0, 20);
    const int command = bitsToInt(bits, 20, 4);
    if (id == 0 || command == 0) {
        return false;
    }

    BitRow clipped = *row;
    clipped.bits = bits;
    clipped.raw = rawForBits(bits.mid(0, 24));
    QVariantMap event = eventBase(modelName, protocol, clipped, frequencyHz, signalLevel);
    event.insert(QStringLiteral("deviceId"), QStringLiteral("0x%1").arg(id, 5, 16, QLatin1Char('0')).toUpper());
    event.insert(QStringLiteral("channel"), QString::number(command));
    event.insert(QStringLiteral("hasBattery"), command == 0x0d || command == 0x0f);
    event.insert(QStringLiteral("batteryOk"), command != 0x0d && command != 0x0f);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    event.insert(QStringLiteral("command"), command);
    event.insert(QStringLiteral("state"), securityCommandText(command));
    finishEvent(&event);
    event.insert(QStringLiteral("summary"),
                 QStringLiteral("ID 0x%1 / %2").arg(id, 5, 16, QLatin1Char('0')).toUpper()
                     + QStringLiteral(" / CMD 0x%1").arg(command, 0, 16).toUpper());
    events->append(event);
    return true;
}

bool appendGenericPwmRaw(const QList<BitRow> &rows,
                         const QString &protocol,
                         QList<QVariantMap> *events,
                         double frequencyHz,
                         double signalLevel)
{
    const BitRow *best = nullptr;
    for (const BitRow &row : rows) {
        if (row.bits.size() < 18 || row.bits.size() > 32 || row.repeats < 2) {
            continue;
        }
        if (best == nullptr || row.repeats > best->repeats || row.bits.size() > best->bits.size()) {
            best = &row;
        }
    }
    if (best == nullptr) {
        return false;
    }

    BitRow clipped = *best;
    clipped.bits = invertedBits(best->bits);
    clipped.raw = rawForBits(clipped.bits);
    QVariantMap event = eventBase(QStringLiteral("Generic-PWM"),
                                  protocol,
                                  clipped,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString());
    event.insert(QStringLiteral("channel"), QString());
    event.insert(QStringLiteral("hasBattery"), false);
    event.insert(QStringLiteral("batteryOk"), false);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    finishEvent(&event);
    events->append(event);
    return true;
}

bool appendGeneric(const QList<BitRow> &rows,
                   const QString &protocol,
                   QList<QVariantMap> *events,
                   double frequencyHz,
                   double signalLevel)
{
    const BitRow *best = nullptr;
    for (const BitRow &row : rows) {
        if (row.bits.size() < 16 || row.bits.size() > 96 || row.repeats < 2) {
            continue;
        }
        if (best == nullptr || row.repeats > best->repeats || row.bits.size() > best->bits.size()) {
            best = &row;
        }
    }
    if (best == nullptr) {
        return false;
    }

    QVariantMap event = eventBase(QStringLiteral("Generic-OOK"),
                                  protocol,
                                  *best,
                                  frequencyHz,
                                  signalLevel);
    event.insert(QStringLiteral("deviceId"), QString());
    event.insert(QStringLiteral("channel"), QString());
    event.insert(QStringLiteral("hasBattery"), false);
    event.insert(QStringLiteral("batteryOk"), false);
    event.insert(QStringLiteral("hasTemperature"), false);
    event.insert(QStringLiteral("temperatureC"), 0.0);
    event.insert(QStringLiteral("hasHumidity"), false);
    event.insert(QStringLiteral("humidity"), 0);
    finishEvent(&event);
    events->append(event);
    return true;
}

bool appendTpmsFromRows(const QList<BitRow> &rows,
                        QList<QVariantMap> *events,
                        double frequencyHz,
                        double signalLevel)
{
    TpmsDecoder decoder;

    for (const BitRow &row : rows) {
        if (row.bits.size() < 48 || row.bits.size() > 128) {
            continue;
        }

        std::vector<uint8_t> bits;
        bits.reserve(static_cast<size_t>(row.bits.size()));
        for (int bit : row.bits) {
            bits.push_back(static_cast<uint8_t>(bit != 0 ? 1 : 0));
        }

        const TpmsFrame frame = decoder.decode(bits);
        if (!frame.valid) {
            continue;
        }
        if (frame.protocol == "Generic TPMS") {
            continue;
        }

        QVariantMap event = eventBase(QStringLiteral("TPMS-%1").arg(QString::fromStdString(frame.protocol)),
                                      QStringLiteral("433/315 MHz TPMS"),
                                      row,
                                      frequencyHz,
                                      signalLevel);
        event.insert(QStringLiteral("category"), QStringLiteral("tpms"));
        event.insert(QStringLiteral("deviceId"),
                     QStringLiteral("0x%1").arg(frame.sensorId, 8, 16, QLatin1Char('0')).toUpper());
        event.insert(QStringLiteral("channel"), QString::fromStdString(frame.wheelName));
        event.insert(QStringLiteral("hasBattery"), true);
        event.insert(QStringLiteral("batteryOk"), !frame.batteryLow);
        event.insert(QStringLiteral("hasTemperature"), true);
        event.insert(QStringLiteral("temperatureC"), frame.temperatureC);
        event.insert(QStringLiteral("hasHumidity"), false);
        event.insert(QStringLiteral("humidity"), 0);
        event.insert(QStringLiteral("pressureKPa"), static_cast<double>(frame.pressureKpa));
        event.insert(QStringLiteral("pressurePSI"), static_cast<double>(frame.pressurePsi));
        event.insert(QStringLiteral("pressureBar"), static_cast<double>(frame.pressureKpa) / 100.0);
        event.insert(QStringLiteral("lowPressure"), frame.pressureAlert);
        event.insert(QStringLiteral("leakAlert"), frame.leakAlert);
        event.insert(QStringLiteral("raw"), QString::fromStdString(frame.rawHex).remove(QLatin1Char(' ')));
        finishEvent(&event);
        event.insert(QStringLiteral("summary"),
                     QStringLiteral("ID %1 / %2 kPa / %3 PSI / %4 C%5")
                         .arg(event.value(QStringLiteral("deviceId")).toString())
                         .arg(static_cast<double>(frame.pressureKpa), 0, 'f', 1)
                         .arg(static_cast<double>(frame.pressurePsi), 0, 'f', 1)
                         .arg(frame.temperatureC)
                         .arg(frame.batteryLow ? QStringLiteral(" / 电池低") : QString()));
        events->append(event);
        return true;
    }

    return false;
}

void applyCategoryDefaults(QList<QVariantMap> *events)
{
    for (QVariantMap &event : *events) {
        if (!event.value(QStringLiteral("category")).toString().isEmpty()) {
            continue;
        }

        const QString modelName = event.value(QStringLiteral("modelName")).toString();
        const QString protocol = event.value(QStringLiteral("protocol")).toString();

        if (modelName.contains(QStringLiteral("Car"), Qt::CaseInsensitive)
                || modelName.contains(QStringLiteral("Remote"), Qt::CaseInsensitive)
                || modelName.contains(QStringLiteral("RKE"), Qt::CaseInsensitive)
                || protocol.contains(QStringLiteral("KeeLoq"), Qt::CaseInsensitive)
                || protocol.contains(QStringLiteral("EV1527"), Qt::CaseInsensitive)
                || protocol.contains(QStringLiteral("SC226"), Qt::CaseInsensitive)) {
            event.insert(QStringLiteral("category"), QStringLiteral("car_key"));
        } else if (event.value(QStringLiteral("hasTemperature")).toBool()
                   || event.value(QStringLiteral("hasHumidity")).toBool()) {
            event.insert(QStringLiteral("category"), QStringLiteral("sensor"));
        } else {
            event.insert(QStringLiteral("category"), QStringLiteral("generic"));
        }
    }
}
}

QList<QVariantMap> SensorDecoder::decode(const std::vector<SensorPulse> &pulses,
                                         double frequencyHz,
                                         double signalLevel) const
{
    QList<QVariantMap> events;
    if (pulses.size() < 16) {
        return events;
    }

    const QList<BitRow> ppmOneTwoRows = ppmRows(pulses, 1000, 2000, 3000);
    appendAcurite609(ppmOneTwoRows, &events, frequencyHz, signalLevel);
    appendBaldr(ppmOneTwoRows, &events, frequencyHz, signalLevel);
    appendNexus(ppmOneTwoRows, &events, frequencyHz, signalLevel);

    const QList<BitRow> ppmTwoFourRows = ppmRows(pulses, 2000, 4000, 7000);
    appendThermoProTx2(ppmTwoFourRows, &events, frequencyHz, signalLevel);

    const QList<BitRow> hcs200Rows = pwmRows(pulses, 370, 772, 1500);
    const QList<BitRow> hcs200Frames = fixedWidthRows(hcs200Rows, 66);
    bool decodedRke = appendHcs200(hcs200Frames, &events, frequencyHz, signalLevel);

    const QList<BitRow> hcs361Sync833Rows = pwmRows(pulses, 400, 800, 1200);
    const QList<BitRow> hcs361Sync833Frames = fixedWidthRows(hcs361Sync833Rows, 67);
    if (!decodedRke) {
        decodedRke = appendHcs361(hcs361Sync833Frames,
                                  QStringLiteral("OOK PWM KeeLoq HCS361 833 bps"),
                                  &events,
                                  frequencyHz,
                                  signalLevel);
    }

    const QList<BitRow> hcs361Sync1667Rows = pwmRows(pulses, 200, 400, 600);
    const QList<BitRow> hcs361Sync1667Frames = fixedWidthRows(hcs361Sync1667Rows, 67);
    if (!decodedRke) {
        decodedRke = appendHcs361(hcs361Sync1667Frames,
                                  QStringLiteral("OOK PWM KeeLoq HCS361 1667 bps"),
                                  &events,
                                  frequencyHz,
                                  signalLevel);
    }

    const QList<BitRow> hcs361NoSync833Rows = pwmRows(pulses, 200, 400, 1200);
    const QList<BitRow> hcs361NoSync833Frames = fixedWidthRows(hcs361NoSync833Rows, 67);
    if (!decodedRke) {
        decodedRke = appendHcs361(hcs361NoSync833Frames,
                                  QStringLiteral("OOK PWM KeeLoq HCS361 no-sync"),
                                  &events,
                                  frequencyHz,
                                  signalLevel);
    }

    const QList<BitRow> hcs361NoSync1667Rows = pwmRows(pulses, 100, 200, 600);
    const QList<BitRow> hcs361NoSync1667Frames = fixedWidthRows(hcs361NoSync1667Rows, 67);
    if (!decodedRke) {
        decodedRke = appendHcs361(hcs361NoSync1667Frames,
                                  QStringLiteral("OOK PWM KeeLoq HCS361 fast"),
                                  &events,
                                  frequencyHz,
                                  signalLevel);
    }

    const QList<BitRow> chryslerRows = pwmRows(pulses, 350, 700, 4200);
    QList<BitRow> chryslerFrames = fixedWidthRows(chryslerRows, 48);
    chryslerFrames.append(fixedWidthRows(chryslerRows, 49));
    if (!decodedRke) {
        decodedRke = appendChryslerRemote(chryslerFrames, &events, frequencyHz, signalLevel);
    }

    const QList<BitRow> audiovoxRows = pwmRows(pulses, 445, 895, 1790);
    const QList<BitRow> audiovoxFrames = fixedWidthRows(audiovoxRows, 25);
    if (!decodedRke) {
        decodedRke = appendAudiovoxRemote(audiovoxFrames, &events, frequencyHz, signalLevel);
    }

    // Toyota / Subaru RKE：覆盖 Corolla 2014+（383/766 µs, 56 bit）及旧款（40/48 bit）
    const QList<BitRow> toyotaRows = pwmRows(pulses, 300, 820, 3000);
    if (!decodedRke) {
        decodedRke =
            appendToyotaRemote(fixedWidthRows(toyotaRows, 56), 56, &events, frequencyHz, signalLevel)
            || appendToyotaRemote(fixedWidthRows(toyotaRows, 48), 48, &events, frequencyHz, signalLevel)
            || appendToyotaRemote(fixedWidthRows(toyotaRows, 40), 40, &events, frequencyHz, signalLevel);
    }

    const QList<BitRow> genericRemoteRows = pwmRows(pulses, 464, 1404, 1800);
    if (!decodedRke) {
        appendSecurityPwm(genericRemoteRows,
                          QStringLiteral("Generic-Remote"),
                          QStringLiteral("OOK PWM EV1527/SC226x"),
                          2,
                          &events,
                          frequencyHz,
                          signalLevel);
    }

    const QList<BitRow> keruiRows = pwmRows(pulses, 420, 960, 1100);
    if (!decodedRke) {
        appendSecurityPwm(keruiRows,
                          QStringLiteral("Kerui-Security"),
                          QStringLiteral("OOK PWM x1527"),
                          3,
                          &events,
                          frequencyHz,
                          signalLevel);
    }

    const QList<BitRow> chuangoRows = pwmRows(pulses, 568, 1704, 1800);
    if (!decodedRke) {
        appendSecurityPwm(chuangoRows,
                          QStringLiteral("Chuango-Security"),
                          QStringLiteral("OOK PWM x1527"),
                          1,
                          &events,
                          frequencyHz,
                          signalLevel);
    }

    if (events.isEmpty()) {
        appendGenericPwmRaw(genericRemoteRows, QStringLiteral("OOK PWM EV1527/SC226x"), &events, frequencyHz, signalLevel)
            || appendGenericPwmRaw(keruiRows, QStringLiteral("OOK PWM x1527"), &events, frequencyHz, signalLevel)
            || appendGenericPwmRaw(chuangoRows, QStringLiteral("OOK PWM x1527"), &events, frequencyHz, signalLevel)
            || appendBestRepeatedWindow(hcs200Rows,
                                        QList<int>{56, 40, 66, 67, 64, 96, 128},
                                        &events,
                                        QStringLiteral("OOK PWM RKE/KeeLoq-like"),
                                        frequencyHz,
                                        signalLevel)
            || appendBestRepeatedWindow(hcs361Sync833Rows,
                                        QList<int>{67, 66, 64, 96, 128},
                                        &events,
                                        QStringLiteral("OOK PWM RKE/KeeLoq-like"),
                                        frequencyHz,
                                        signalLevel)
            || appendBestRepeatedWindow(hcs361NoSync1667Rows,
                                        QList<int>{67, 66, 64, 96, 128},
                                        &events,
                                        QStringLiteral("OOK PWM RKE/KeeLoq-like"),
                                        frequencyHz,
                                        signalLevel)
            || appendBestRepeatedWindow(chryslerRows,
                                        QList<int>{48, 49, 64, 66, 67},
                                        &events,
                                        QStringLiteral("OOK PWM car remote-like"),
                                        frequencyHz,
                                        signalLevel)
            || appendGenericRkeRaw(hcs200Rows, QStringLiteral("OOK PWM RKE/KeeLoq-like"), &events, frequencyHz, signalLevel)
            || appendGenericRkeRaw(hcs361Sync833Rows, QStringLiteral("OOK PWM RKE/KeeLoq-like"), &events, frequencyHz, signalLevel)
            || appendGenericRkeRaw(hcs361NoSync1667Rows, QStringLiteral("OOK PWM RKE/KeeLoq-like"), &events, frequencyHz, signalLevel)
            || appendGenericRkeRaw(chryslerRows, QStringLiteral("OOK PWM car remote-like"), &events, frequencyHz, signalLevel);
    }

    const QList<BitRow> tpmsFastRows = pwmRows(pulses, 50, 100, 3000);
    const QList<BitRow> tpmsMediumRows = pwmRows(pulses, 100, 200, 4000);
    if (events.isEmpty()) {
        appendTpmsFromRows(fixedWidthRows(tpmsFastRows, 56), &events, frequencyHz, signalLevel)
            || appendTpmsFromRows(fixedWidthRows(tpmsFastRows, 64), &events, frequencyHz, signalLevel)
            || appendTpmsFromRows(fixedWidthRows(tpmsFastRows, 72), &events, frequencyHz, signalLevel)
            || appendTpmsFromRows(fixedWidthRows(tpmsMediumRows, 56), &events, frequencyHz, signalLevel)
            || appendTpmsFromRows(fixedWidthRows(tpmsMediumRows, 64), &events, frequencyHz, signalLevel)
            || appendTpmsFromRows(fixedWidthRows(tpmsMediumRows, 72), &events, frequencyHz, signalLevel);
    }

    if (events.isEmpty()) {
        appendGeneric(ppmOneTwoRows, QStringLiteral("OOK PPM 1/2 ms"), &events, frequencyHz, signalLevel)
            || appendGeneric(ppmTwoFourRows, QStringLiteral("OOK PPM 2/4 ms"), &events, frequencyHz, signalLevel);
    }

    applyCategoryDefaults(&events);
    return events;
}
