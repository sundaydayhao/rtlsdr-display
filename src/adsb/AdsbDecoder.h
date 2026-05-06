#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include <QString>

struct DecodedAdsbMessage
{
    QString icao;
    QString callsign;

    bool hasAltitude = false;
    int altitudeFeet = 0;

    bool hasCpr = false;
    bool cprOdd = false;
    int cprLatitude = 0;
    int cprLongitude = 0;

    bool hasSpeed = false;
    double speedKnots = 0.0;

    bool hasHeading = false;
    double headingDegrees = 0.0;
};

class AdsbDecoder
{
public:
    struct BlockStats {
        quint64 candidates = 0;
        quint64 validMessages = 0;
    };

    using MessageCallback = std::function<void(const std::array<uint8_t, 14> &)>;

    BlockStats processMagnitudes(const std::vector<uint32_t> &magnitudes,
                                 int scanStart,
                                 const MessageCallback &onMessage) const;

    static bool decodeMessage(const std::array<uint8_t, 14> &message, DecodedAdsbMessage *decoded);
    static bool decodeGlobalCpr(bool newestOdd,
                                int evenLat,
                                int evenLon,
                                int oddLat,
                                int oddLon,
                                double *latitude,
                                double *longitude);

private:
    static bool hasModeSPreamble(const std::vector<uint32_t> &magnitudes, int offset);
    static bool checksumIsValid(const std::array<uint8_t, 14> &message);
    static int getBits(const std::array<uint8_t, 14> &message, int firstBit, int bitCount);
    static QString decodeCallsign(const std::array<uint8_t, 14> &message);
    static bool decodeAltitude(int altitudeCode, int *altitudeFeet);
    static int cprMod(int value, int modulo);
    static int cprNL(double latitude);
};
