#include "TpmsDecoder.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 24) |
                  (static_cast<uint32_t>(bytes[1]) << 16) |
                  (static_cast<uint32_t>(bytes[2]) << 8)  |
                   static_cast<uint32_t>(bytes[3]);

    // Sanity check: ID should not be all zeros or all ones
    if (id == 0 || id == 0xFFFFFFFF) return frame;

    uint8_t flags       = bytes[4];
    uint8_t pressureRaw = bytes[5];
    uint8_t tempRaw     = bytes[6];

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
    if (frame.pressureKpa < 0 || frame.pressureKpa > 600) return TpmsFrame{};
    if (frame.temperatureC < -40 || frame.temperatureC > 125) return TpmsFrame{};

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

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 24) |
                  (static_cast<uint32_t>(bytes[1]) << 16) |
                  (static_cast<uint32_t>(bytes[2]) << 8)  |
                   static_cast<uint32_t>(bytes[3]);

    if (id == 0 || id == 0xFFFFFFFF) return frame;

    uint8_t status      = bytes[4];
    uint8_t pressureRaw = bytes[5];
    uint8_t tempRaw     = bytes[6];

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

    if (frame.pressureKpa < 0 || frame.pressureKpa > 600) return TpmsFrame{};
    if (frame.temperatureC < -50 || frame.temperatureC > 125) return TpmsFrame{};

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

    uint32_t id = (static_cast<uint32_t>(bytes[0]) << 16) |
                  (static_cast<uint32_t>(bytes[1]) << 8)  |
                   static_cast<uint32_t>(bytes[2]);

    if (id == 0 || id == 0xFFFFFF) return frame;

    uint8_t pressureRaw = bytes[3];
    uint8_t tempRaw     = bytes[4];
    uint8_t status      = bytes[5];

    frame.valid         = true;
    frame.protocol      = "Pacific";
    frame.sensorId      = id;
    frame.pressureKpa   = pressureRaw * 1.5f;
    frame.pressurePsi   = frame.pressureKpa * 0.14504f;
    frame.temperatureC  = static_cast<int>(tempRaw) - 40;
    frame.batteryLow    = (status & 0x80) != 0;
    frame.pressureAlert = (status & 0x40) != 0;
    frame.rawHex        = toHex(bytes, len);

    if (frame.pressureKpa < 0 || frame.pressureKpa > 600) return TpmsFrame{};
    if (frame.temperatureC < -40 || frame.temperatureC > 125) return TpmsFrame{};

    return frame;
}

// ---------------------------------------------------------------------------
// Generic TPMS fallback
// Tries to extract sensor ID and basic pressure/temperature
// ---------------------------------------------------------------------------
TpmsFrame TpmsDecoder::decodeGeneric(const uint8_t* bytes, size_t len)
{
    TpmsFrame frame;
    if (len < 6) return frame;

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
    if (pressureKpa < 50 || pressureKpa > 500) return frame;
    if (tempC < -40 || tempC > 125) return frame;

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