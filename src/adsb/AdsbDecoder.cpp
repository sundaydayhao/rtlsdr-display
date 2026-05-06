#include "AdsbDecoder.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kPreambleSamples = 16;
constexpr int kLongMessageBits = 112;
constexpr int kLongMessageSamples = kPreambleSamples + kLongMessageBits * 2;
constexpr uint32_t kModeSGenerator = 0x1FFF409;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCprScale = 131072.0;
}

AdsbDecoder::BlockStats AdsbDecoder::processMagnitudes(const std::vector<uint32_t> &magnitudes,
                                                       int scanStart,
                                                       const MessageCallback &onMessage) const
{
    BlockStats stats;
    if (magnitudes.size() < kLongMessageSamples) {
        return stats;
    }

    const int end = static_cast<int>(magnitudes.size()) - kLongMessageSamples;
    for (int offset = std::max(0, scanStart); offset <= end; ++offset) {
        if (!hasModeSPreamble(magnitudes, offset)) {
            continue;
        }

        ++stats.candidates;

        std::array<uint8_t, 14> message {};
        for (int bit = 0; bit < kLongMessageBits; ++bit) {
            const int sample = offset + kPreambleSamples + bit * 2;
            const bool one = magnitudes[sample] > magnitudes[sample + 1];
            if (one) {
                message[bit / 8] |= static_cast<uint8_t>(1U << (7 - (bit % 8)));
            }
        }

        const int downlinkFormat = message[0] >> 3;
        if ((downlinkFormat == 17 || downlinkFormat == 18) && checksumIsValid(message)) {
            ++stats.validMessages;
            onMessage(message);
            offset += kLongMessageSamples - 1;
        }
    }

    return stats;
}

bool AdsbDecoder::decodeMessage(const std::array<uint8_t, 14> &message, DecodedAdsbMessage *decoded)
{
    if (decoded == nullptr) {
        return false;
    }

    const int downlinkFormat = message[0] >> 3;
    if (downlinkFormat != 17 && downlinkFormat != 18) {
        return false;
    }

    DecodedAdsbMessage result;
    result.icao = QStringLiteral("%1%2%3")
                      .arg(message[1], 2, 16, QLatin1Char('0'))
                      .arg(message[2], 2, 16, QLatin1Char('0'))
                      .arg(message[3], 2, 16, QLatin1Char('0'))
                      .toUpper();

    const int typeCode = getBits(message, 32, 5);
    if (typeCode >= 1 && typeCode <= 4) {
        result.callsign = decodeCallsign(message);
    } else if (typeCode >= 9 && typeCode <= 18) {
        result.hasAltitude = decodeAltitude(getBits(message, 40, 12), &result.altitudeFeet);
        result.hasCpr = true;
        result.cprOdd = getBits(message, 53, 1) != 0;
        result.cprLatitude = getBits(message, 54, 17);
        result.cprLongitude = getBits(message, 71, 17);
    } else if (typeCode == 19) {
        const int subtype = getBits(message, 37, 3);
        if (subtype == 1 || subtype == 2) {
            int eastWestVelocity = getBits(message, 46, 10);
            int northSouthVelocity = getBits(message, 57, 10);
            if (eastWestVelocity > 0 && northSouthVelocity > 0) {
                --eastWestVelocity;
                --northSouthVelocity;
                if (getBits(message, 45, 1) != 0) {
                    eastWestVelocity = -eastWestVelocity;
                }
                if (getBits(message, 56, 1) != 0) {
                    northSouthVelocity = -northSouthVelocity;
                }

                result.speedKnots = std::sqrt(static_cast<double>(eastWestVelocity * eastWestVelocity
                                                                  + northSouthVelocity * northSouthVelocity));
                if (subtype == 2) {
                    result.speedKnots *= 4.0;
                }
                result.headingDegrees = std::atan2(static_cast<double>(eastWestVelocity),
                                                   static_cast<double>(northSouthVelocity)) * 180.0 / kPi;
                if (result.headingDegrees < 0.0) {
                    result.headingDegrees += 360.0;
                }
                result.hasSpeed = true;
                result.hasHeading = true;
            }
        } else if (subtype == 3 || subtype == 4) {
            if (getBits(message, 45, 1) != 0) {
                result.headingDegrees = getBits(message, 46, 10) * 360.0 / 1024.0;
                result.hasHeading = true;
            }
            int airspeed = getBits(message, 57, 10);
            if (airspeed > 0) {
                --airspeed;
                result.speedKnots = subtype == 4 ? airspeed * 4.0 : airspeed;
                result.hasSpeed = true;
            }
        }
    }

    *decoded = result;
    return true;
}

bool AdsbDecoder::decodeGlobalCpr(bool newestOdd,
                                  int evenLat,
                                  int evenLon,
                                  int oddLat,
                                  int oddLon,
                                  double *latitude,
                                  double *longitude)
{
    if (latitude == nullptr || longitude == nullptr) {
        return false;
    }

    const double evenLatCpr = evenLat / kCprScale;
    const double oddLatCpr = oddLat / kCprScale;
    const double evenLonCpr = evenLon / kCprScale;
    const double oddLonCpr = oddLon / kCprScale;

    const int j = static_cast<int>(std::floor(59.0 * evenLatCpr - 60.0 * oddLatCpr + 0.5));
    double rlatEven = 6.0 * (cprMod(j, 60) + evenLatCpr);
    double rlatOdd = (360.0 / 59.0) * (cprMod(j, 59) + oddLatCpr);

    if (rlatEven >= 270.0) {
        rlatEven -= 360.0;
    }
    if (rlatOdd >= 270.0) {
        rlatOdd -= 360.0;
    }

    if (cprNL(rlatEven) != cprNL(rlatOdd)) {
        return false;
    }

    const double selectedLat = newestOdd ? rlatOdd : rlatEven;
    const int nl = cprNL(selectedLat);
    const int ni = newestOdd ? std::max(nl - 1, 1) : std::max(nl, 1);
    const int m = static_cast<int>(std::floor(evenLonCpr * (nl - 1) - oddLonCpr * nl + 0.5));
    double selectedLon = (360.0 / ni) * (cprMod(m, ni) + (newestOdd ? oddLonCpr : evenLonCpr));
    if (selectedLon > 180.0) {
        selectedLon -= 360.0;
    }

    *latitude = selectedLat;
    *longitude = selectedLon;
    return true;
}

bool AdsbDecoder::hasModeSPreamble(const std::vector<uint32_t> &magnitudes, int offset)
{
    const uint32_t p0 = magnitudes[offset + 0];
    const uint32_t p1 = magnitudes[offset + 2];
    const uint32_t p2 = magnitudes[offset + 7];
    const uint32_t p3 = magnitudes[offset + 9];
    const uint32_t signal = (p0 + p1 + p2 + p3) / 4;
    const uint32_t noise = (magnitudes[offset + 1] + magnitudes[offset + 3]
                            + magnitudes[offset + 4] + magnitudes[offset + 5]
                            + magnitudes[offset + 6] + magnitudes[offset + 8]
                            + magnitudes[offset + 10] + magnitudes[offset + 11]) / 8;

    if (signal < noise + 600 || signal * 2 < noise * 3) {
        return false;
    }

    return p0 > magnitudes[offset + 1]
        && p1 > magnitudes[offset + 1]
        && p1 > magnitudes[offset + 3]
        && p2 > magnitudes[offset + 6]
        && p2 > magnitudes[offset + 8]
        && p3 > magnitudes[offset + 8]
        && p3 > magnitudes[offset + 10];
}

bool AdsbDecoder::checksumIsValid(const std::array<uint8_t, 14> &message)
{
    bool bits[kLongMessageBits] {};
    for (int bit = 0; bit < kLongMessageBits; ++bit) {
        bits[bit] = ((message[bit / 8] >> (7 - (bit % 8))) & 0x01U) != 0;
    }

    for (int bit = 0; bit < kLongMessageBits - 24; ++bit) {
        if (!bits[bit]) {
            continue;
        }

        for (int polyBit = 0; polyBit < 25; ++polyBit) {
            bits[bit + polyBit] = bits[bit + polyBit]
                != (((kModeSGenerator >> (24 - polyBit)) & 0x01U) != 0);
        }
    }

    for (int bit = kLongMessageBits - 24; bit < kLongMessageBits; ++bit) {
        if (bits[bit]) {
            return false;
        }
    }

    return true;
}

int AdsbDecoder::getBits(const std::array<uint8_t, 14> &message, int firstBit, int bitCount)
{
    int value = 0;
    for (int bit = 0; bit < bitCount; ++bit) {
        const int sourceBit = firstBit + bit;
        value = (value << 1) | ((message[sourceBit / 8] >> (7 - (sourceBit % 8))) & 0x01U);
    }
    return value;
}

QString AdsbDecoder::decodeCallsign(const std::array<uint8_t, 14> &message)
{
    QString callsign;
    callsign.reserve(8);

    for (int index = 0; index < 8; ++index) {
        const int code = getBits(message, 40 + index * 6, 6);
        QChar character = QLatin1Char(' ');
        if (code >= 1 && code <= 26) {
            character = QLatin1Char(static_cast<char>('A' + code - 1));
        } else if (code >= 48 && code <= 57) {
            character = QLatin1Char(static_cast<char>('0' + code - 48));
        }
        callsign.append(character);
    }

    return callsign.trimmed();
}

bool AdsbDecoder::decodeAltitude(int altitudeCode, int *altitudeFeet)
{
    if (altitudeFeet == nullptr) {
        return false;
    }

    if ((altitudeCode & 0x10) == 0) {
        return false;
    }

    const int n = ((altitudeCode & 0x0FE0) >> 1) | (altitudeCode & 0x000F);
    *altitudeFeet = n * 25 - 1000;
    return true;
}

int AdsbDecoder::cprMod(int value, int modulo)
{
    int result = value % modulo;
    if (result < 0) {
        result += modulo;
    }
    return result;
}

int AdsbDecoder::cprNL(double latitude)
{
    const double absLat = std::fabs(latitude);
    if (absLat < 1.0e-9) {
        return 59;
    }
    if (absLat >= 87.0) {
        return 1;
    }

    const double numerator = 1.0 - std::cos(kPi / 30.0);
    const double denominator = std::cos(absLat * kPi / 180.0);
    const double value = 1.0 - numerator / (denominator * denominator);
    const double clamped = std::max(-1.0, std::min(1.0, value));
    const int nl = static_cast<int>(std::floor(2.0 * kPi / std::acos(clamped)));
    return std::max(1, std::min(59, nl));
}
