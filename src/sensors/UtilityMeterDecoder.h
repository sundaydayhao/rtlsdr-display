#pragma once
#include "SceneDecoder.h"
#include <vector>
#include <cstdint>

/**
 * @brief 电表 / 水表 / 燃气表无线解码器
 *
 * 支持协议：
 *  1. ERT (Encoder Receiver Transmitter) — 北美 AMR 标准
 *     - SCM  (Standard Consumption Message)  76 bit
 *     - IDM  (Interval Data Message)         92 bit
 *  2. Wireless M-Bus (EN 13757-4) — 欧洲标准
 *     - Mode T (频繁发送，单向)
 *     - Mode C (紧凑型)
 *  3. 国内常见 433 MHz 抄表协议（简化帧）
 *     - 前导 0xAA 0xAA + 同步 0x2D 0xD4 + 地址 4B + 数据 N B + CRC
 */
class UtilityMeterDecoder : public SceneDecoder
{
    Q_OBJECT
public:
    explicit UtilityMeterDecoder(QObject *parent = nullptr);

    bool decode(const std::vector<uint8_t> &bits, double freq) override;
    QString sceneName() const override { return QStringLiteral("电表/水表/燃气表"); }

private:
    // ERT SCM 解码
    bool decodeERT_SCM(const std::vector<uint8_t> &bits, QVariantMap &result);
    // Wireless M-Bus Mode T/C 解码
    bool decodeWMBus(const std::vector<uint8_t> &bits, QVariantMap &result);
    // 国内 433 MHz 抄表协议
    bool decodeDomestic433(const std::vector<uint8_t> &bits, QVariantMap &result);

    // 比特流转字节数组（MSB first）
    static std::vector<uint8_t> bitsToBytes(const std::vector<uint8_t> &bits, int start, int byteCount);
    // CRC-16/CCITT 校验
    static uint16_t crc16(const std::vector<uint8_t> &data, int start, int len);
    // 曼彻斯特解码
    static bool manchesterDecode(const std::vector<uint8_t> &bits, std::vector<uint8_t> &decoded);
    // 字节数组转十六进制字符串
    static QString bytesToHex(const std::vector<uint8_t> &bytes);
    // 比特转无符号整数
    static uint32_t bitsToUint(const std::vector<uint8_t> &bits, int start, int len);

    // 仪表类型字符串
    static QString meterTypeStr(uint8_t type);
};