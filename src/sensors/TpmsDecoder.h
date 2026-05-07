#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief 汽车胎压传感器 (TPMS) 解码器
 *
 * 支持协议：
 *  1. Schrader TPMS  - 最常见的 OEM 胎压传感器（315/433 MHz）
 *  2. Continental TPMS - 欧洲车型常用
 *  3. Pacific Industries TPMS - 日系车型
 *  4. Generic TPMS - 通用格式
 *
 * 典型帧结构：
 *   前导码 + 同步 + 传感器ID(4B) + 状态(1B) + 气压(1B) + 温度(1B) + CRC(1-2B)
 *
 * 气压单位：通常为 kPa/4 或 PSI/0.25
 * 温度单位：°C + 40 偏移
 */

enum class TpmsWheel {
    Unknown = 0,
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight
};

struct TpmsFrame {
    bool        valid         = false;
    std::string protocol;           // "Schrader", "Continental", "Pacific", "Generic"
    uint32_t    sensorId      = 0;  // 传感器唯一 ID
    float       pressureKpa   = 0;  // 气压 (kPa)
    float       pressurePsi   = 0;  // 气压 (PSI)
    int         temperatureC  = 0;  // 温度 (°C)
    bool        batteryLow    = false;
    bool        pressureAlert = false;  // 气压异常报警
    bool        leakAlert     = false;  // 漏气报警
    TpmsWheel   wheel         = TpmsWheel::Unknown;
    std::string wheelName;
    std::string rawHex;
};

class TpmsDecoder
{
public:
    TpmsDecoder() = default;

    // 从比特流解码（每个元素为 0 或 1）
    TpmsFrame decode(const std::vector<uint8_t>& bits);

    // 从字节数组解码
    TpmsFrame decodeBytes(const uint8_t* bytes, size_t len);

    // 获取车轮名称
    static std::string wheelName(TpmsWheel wheel);

private:
    TpmsFrame decodeSchrader(const uint8_t* bytes, size_t len);
    TpmsFrame decodeContinental(const uint8_t* bytes, size_t len);
    TpmsFrame decodePacific(const uint8_t* bytes, size_t len);
    TpmsFrame decodeGeneric(const uint8_t* bytes, size_t len);

    static std::vector<uint8_t> bitsToBytes(const std::vector<uint8_t>& bits);
    static std::string toHex(const uint8_t* data, size_t len);
    static uint8_t crc8(const uint8_t* data, size_t len, uint8_t poly = 0x07, uint8_t init = 0x00);
    static uint16_t crc16(const uint8_t* data, size_t len);
    static bool validateCrc8(const uint8_t* data, size_t len);
};