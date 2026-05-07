#include "TpmsDecoder.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {
int bitCount(uint8_t value)
{
    int count = 0;
    while (value != 0) {
        count += value & 1U;
        value = static_cast<uint8_t>(value >> 1);
    }
    return count;
}

bool plausibleGenericPayload(const uint8_t* bytes, size_t len)
{
    if (bytes == nullptr || len < 8) {
        return false;
    }

    int oneBits = 0;
    int extremeBytes = 0;
    const size_t inspected = std::min<size_t>(len, 8);
    for (size_t index = 0; index < inspected; ++index) {
        oneBits += bitCount(bytes[index]);
        if (bytes[index] <= 0x0F || bytes[index] >= 0xF0) {
            ++extremeBytes;
        }
    }

    const double oneRatio = static_cast<double>(oneBits) / static_cast<double>(inspected * 8);
    if (oneRatio < 0.25 || oneRatio > 0.75) {
        return false;
    }
    if (extremeBytes >= 3) {
        return false;
    }

    const uint8_t pressureRaw = bytes[4];
    const uint8_t temperatureRaw = bytes[5];
    if (pressureRaw <= 0x03 || pressureRaw >= 0xF0) {
        return false;
    }
    if (temperatureRaw >= 0xF0) {
        return false;
    }

    return true;
}

bool fillPatternLikeRemoteWindow(const uint8_t* bytes, size_t len)
{
    if (bytes == nullptr || len == 0) {
        return true;
    }

    int fillBytes = 0;
    const size_t inspected = std::min<size_t>(len, 9);
    for (size_t index = 0; index < inspected; ++index) {
        if (bytes[index] <= 0x03 || bytes[index] >= 0xFD) {
            ++fillBytes;
        }
    }

    return fillBytes >= 3;
}

bool plausibleTpmsMeasurement(float pressureKpa, int temperatureC)
{
    return pressureKpa >= 90.0f
        && pressureKpa <= 350.0f
        && temperatureC >= -40
        && temperatureC <= 80;
}

bool plausibleTpmsStatus(uint8_t status)
{
    if (status == 0xFF) {
        return false;
    }
    if ((status & 0x0F) == 0x0F || (status & 0xF0) == 0xF0) {
        return false;
    }
    return true;
}
}

std::string TpmsDecoder::toHex(const uint8_t* data, size_t len)
{
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);
        if (i + 1 < len) oss << ' ';
    }
    return oss.str();
}

std::vector<uint8_t> TpmsDecoder::bitsToBytes(const std::vector<uint8_t>& bits)
{
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 7 < bits.size(); i += 8) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b)
            byte = static_cast<uint8_t>((byte << 1) | (bits[i + b] & 1));
        bytes.push_back(byte);
    }
    return bytes;
}

uint8_t TpmsDecoder::crc8(const uint8_t* data, size_t len, uint8_t poly, uint8_t init)
{
    uint8_t crc = init;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80)
                crc = static_cast<uint8_t>((crc << 1) ^ poly);
            else
                crc = static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

uint16_t TpmsDecoder::crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i] << 8);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000)
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

bool TpmsDecoder::validateCrc8(const uint8_t* data, size_t len)
{
    if (len < 2) return false;
    uint8_t calc = crc8(data, len - 1);
    return calc == data[len - 1];
}

std::string TpmsDecoder::wheelName(TpmsWheel wheel)
{
    switch (wheel) {
    case TpmsWheel::FrontLeft:  return "左前轮";
    case TpmsWheel::FrontRight: return "右前轮";
    case TpmsWheel::RearLeft:   return "左后轮";
    case TpmsWheel::RearRight:  return "右后轮";
    default:                    return "未知轮位";
    }
}

// ---------------------------------------------------------------------------
// Schrader TPMS Protocol
// Frame (8 bytes): ID[3:0] | Flags | Pressure | Temperature | CRC
// Pressure: value * 0.25 kPa (or value * 0.36 PSI depending on variant)
// Temperature: value - 40 °C
// Flags: bit7=battery_low, bit6=pressure_alert, bit5=leak_alert
// ---------------------------------------------------------------------------
TpmsFrame TpmsDecoder::decodeSchrader(const uint8_t* bytes, size_t len)
{
    TpmsFrame frame;
    if (len < 8) return frame;

    // Validate CRC
    if (!validateCrc8(bytes, 8)) return frame;
    if (fillPatternLikeRemoteWindow(bytes, 8)) return frame;

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 24) |
                  (static_cast<uint32_t>(bytes[1]) << 16) |
                  (static_cast<uint32_t>(bytes[2]) << 8)  |
                   static_cast<uint32_t>(bytes[3]);

    // Sanity check: ID should not be all zeros or all ones
    if (id == 0 || id == 0xFFFFFFFF) return frame;

    uint8_t flags       = bytes[4];
    uint8_t pressureRaw = bytes[5];
    uint8_t tempRaw     = bytes[6];
    if (!plausibleTpmsStatus(flags)) return TpmsFrame{};

    frame.valid         = true;
    frame.protocol      = "Schrader";
    frame.sensorId      = id;
    frame.pressureKpa   = pressureRaw * 0.25f;
    frame.pressurePsi   = frame.pressureKpa * 0.14504f;
    frame.temperatureC  = static_cast<int>(tempRaw) - 40;
    frame.batteryLow    = (flags & 0x80) != 0;
    frame.pressureAlert = (flags & 0x40) != 0;
    frame.leakAlert     = (flags & 0x20) != 0;
    frame.rawHex        = toHex(bytes, len);

    // Sanity checks
    if (!plausibleTpmsMeasurement(frame.pressureKpa, frame.temperatureC)) return TpmsFrame{};

    return frame;
}

// ---------------------------------------------------------------------------
// Continental TPMS Protocol
// Frame (9 bytes): ID[3:0] | Status | Pressure | Temperature | Counter | CRC
// Pressure: value * 1.364 kPa (raw value in 1/4 PSI units)
// Temperature: value - 50 °C
// ---------------------------------------------------------------------------
TpmsFrame TpmsDecoder::decodeContinental(const uint8_t* bytes, size_t len)
{
    TpmsFrame frame;
    if (len < 9) return frame;

    // CRC check (CRC-8/AUTOSAR poly=0x2F)
    uint8_t calcCrc = crc8(bytes, 8, 0x2F, 0xFF);
    if (calcCrc != bytes[8]) return frame;
    if (fillPatternLikeRemoteWindow(bytes, 9)) return frame;

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 24) |
                  (static_cast<uint32_t>(bytes[1]) << 16) |
                  (static_cast<uint32_t>(bytes[2]) << 8)  |
                   static_cast<uint32_t>(bytes[3]);

    if (id == 0 || id == 0xFFFFFFFF) return frame;

    uint8_t status      = bytes[4];
    uint8_t pressureRaw = bytes[5];
    uint8_t tempRaw     = bytes[6];
    if (!plausibleTpmsStatus(status)) return TpmsFrame{};

    frame.valid         = true;
    frame.protocol      = "Continental";
    frame.sensorId      = id;
    // Continental: pressure in 1/4 PSI units
    frame.pressurePsi   = pressureRaw * 0.25f;
    frame.pressureKpa   = frame.pressurePsi * 6.89476f;
    frame.temperatureC  = static_cast<int>(tempRaw) - 50;
    frame.batteryLow    = (status & 0x04) != 0;
    frame.pressureAlert = (status & 0x02) != 0;
    frame.leakAlert     = (status & 0x01) != 0;
    frame.rawHex        = toHex(bytes, len);

    if (!plausibleTpmsMeasurement(frame.pressureKpa, frame.temperatureC)) return TpmsFrame{};

    return frame;
}

// ---------------------------------------------------------------------------
// Pacific Industries TPMS Protocol (Japanese OEM)
// Frame (7 bytes): ID[2:0] | Pressure | Temperature | Status | CRC
// Pressure: value * 1.5 kPa
// Temperature: value - 40 °C
// ---------------------------------------------------------------------------
TpmsFrame TpmsDecoder::decodePacific(const uint8_t* bytes, size_t len)
{
    TpmsFrame frame;
    if (len < 7) return frame;

    if (!validateCrc8(bytes, 7)) return frame;
    if (fillPatternLikeRemoteWindow(bytes, 7)) return frame;

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 16) |
                  (static_cast<uint32_t>(bytes[1]) << 8)  |
                   static_cast<uint32_t>(bytes[2]);

    if (id == 0 || id == 0xFFFFFF) return frame;

    uint8_t pressureRaw = bytes[3];
    uint8_t tempRaw     = bytes[4];
    uint8_t status      = bytes[5];
    if (!plausibleTpmsStatus(status)) return TpmsFrame{};

    frame.valid         = true;
    frame.protocol      = "Pacific";
    frame.sensorId      = id;
    frame.pressureKpa   = pressureRaw * 1.5f;
    frame.pressurePsi   = frame.pressureKpa * 0.14504f;
    frame.temperatureC  = static_cast<int>(tempRaw) - 40;
    frame.batteryLow    = (status & 0x80) != 0;
    frame.pressureAlert = (status & 0x40) != 0;
    frame.rawHex        = toHex(bytes, len);

    if (!plausibleTpmsMeasurement(frame.pressureKpa, frame.temperatureC)) return TpmsFrame{};

    return frame;
}

// ---------------------------------------------------------------------------
// Generic TPMS fallback
// Tries to extract sensor ID and basic pressure/temperature
// ---------------------------------------------------------------------------
TpmsFrame TpmsDecoder::decodeGeneric(const uint8_t* bytes, size_t len)
{
    TpmsFrame frame;
    if (!plausibleGenericPayload(bytes, len)) return frame;

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 24) |
                  (static_cast<uint32_t>(bytes[1]) << 16) |
                  (static_cast<uint32_t>(bytes[2]) << 8)  |
                   static_cast<uint32_t>(bytes[3]);

    if (id == 0 || id == 0xFFFFFFFF) return frame;

    uint8_t pressureRaw = bytes[4];
    uint8_t tempRaw     = (len > 5) ? bytes[5] : 0;

    // Try common pressure scales
    float pressureKpa = pressureRaw * 0.25f;  // Most common scale
    int   tempC       = static_cast<int>(tempRaw) - 40;

    // Sanity check
    if (!plausibleTpmsMeasurement(pressureKpa, tempC)) return frame;

    frame.valid        = true;
    frame.protocol     = "Generic TPMS";
    frame.sensorId     = id;
    frame.pressureKpa  = pressureKpa;
    frame.pressurePsi  = pressureKpa * 0.14504f;
    frame.temperatureC = tempC;
    frame.rawHex       = toHex(bytes, len);
    return frame;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

TpmsFrame TpmsDecoder::decodeBytes(const uint8_t* bytes, size_t len)
{
    // Try Schrader (most common)
    TpmsFrame frame = decodeSchrader(bytes, len);
    if (frame.valid) return frame;

    // Try Continental
    frame = decodeContinental(bytes, len);
    if (frame.valid) return frame;

    // Try Pacific
    frame = decodePacific(bytes, len);
    if (frame.valid) return frame;

    // Generic fallback
    return decodeGeneric(bytes, len);
}

TpmsFrame TpmsDecoder::decode(const std::vector<uint8_t>& bits)
{
    TpmsFrame frame;
    if (bits.empty()) return frame;

    // Check if input is already bytes (values > 1)
    bool isBytes = false;
    for (auto b : bits) {
        if (b > 1) { isBytes = true; break; }
    }

    if (isBytes) {
        return decodeBytes(bits.data(), bits.size());
    }

    // Convert bit stream to bytes
    auto bytes = bitsToBytes(bits);
    if (bytes.size() < 6) return frame;

    return decodeBytes(bytes.data(), bytes.size());
}
