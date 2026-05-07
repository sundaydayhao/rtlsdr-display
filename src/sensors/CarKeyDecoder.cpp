#include "CarKeyDecoder.h"
#include <QDateTime>
#include <cstring>

CarKeyDecoder::CarKeyDecoder(QObject *parent)
    : SceneDecoder(parent)
{
}

bool CarKeyDecoder::decode(const std::vector<uint8_t> &bits, double freq)
{
    if (bits.size() < 24)
        return false;

    QVariantMap result;
    result["scene"]     = sceneName();
    result["frequency"] = QString("%1 MHz").arg(freq / 1e6, 0, 'f', 3);
    result["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    // 依次尝试各协议
    if (decodeEV1527(bits, result)) {
        emit decoded(result);
        return true;
    }
    if (decodeKeeLoq(bits, result)) {
        emit decoded(result);
        return true;
    }
    if (decodeGenericOOK(bits, result)) {
        emit decoded(result);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// EV1527 / PT2262  24-bit 固定码
// 地址 20 bit + 数据 4 bit
// ─────────────────────────────────────────────────────────────────────────────
bool CarKeyDecoder::decodeEV1527(const std::vector<uint8_t> &bits, QVariantMap &result)
{
    // 需要至少 24 bit
    if (bits.size() < 24)
        return false;

    // EV1527 数据位不含 "11" 连续两个 1（三态编码中 '1' 对应 "10"，'0' 对应 "00"，'f' 对应 "11"）
    // 简化判断：检查是否存在合理的 24 bit 帧
    uint32_t addr = bitsToUint(bits, 0, 20);
    uint8_t  data = static_cast<uint8_t>(bitsToUint(bits, 20, 4));

    result["protocol"]    = QStringLiteral("EV1527/PT2262");
    result["address"]     = QString("0x%1").arg(addr, 5, 16, QChar('0')).toUpper();
    result["data"]        = QString("0x%1").arg(data, 1, 16, QChar('0')).toUpper();
    result["button"]      = buttonDescription(data);
    result["raw_bits"]    = bitsToHex(bits, 0, static_cast<int>(bits.size()));
    result["bit_count"]   = static_cast<int>(bits.size());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// KeeLoq 滚动码  帧结构识别（66 bit：32 bit 加密 + 34 bit 明文）
// ─────────────────────────────────────────────────────────────────────────────
bool CarKeyDecoder::decodeKeeLoq(const std::vector<uint8_t> &bits, QVariantMap &result)
{
    if (bits.size() < 66)
        return false;

    // KeeLoq 帧：66 bit，前 32 bit 为加密跳变码，后 34 bit 为明文
    // 明文部分：序列号 28 bit + 功能 4 bit + 重复标志 1 bit + VLow 1 bit
    uint32_t encHigh = bitsToUint(bits, 0, 16);
    uint32_t encLow  = bitsToUint(bits, 16, 16);
    uint32_t serial  = bitsToUint(bits, 32, 28);
    uint8_t  func    = static_cast<uint8_t>(bitsToUint(bits, 60, 4));
    uint8_t  repeat  = bits[64];
    uint8_t  vlow    = bits[65];

    result["protocol"]    = QStringLiteral("KeeLoq 滚动码");
    result["serial"]      = QString("0x%1").arg(serial, 7, 16, QChar('0')).toUpper();
    result["encrypted"]   = QString("%1%2")
                                .arg(encHigh, 4, 16, QChar('0'))
                                .arg(encLow,  4, 16, QChar('0'))
                                .toUpper();
    result["function"]    = buttonDescription(func);
    result["repeat"]      = repeat ? QStringLiteral("是") : QStringLiteral("否");
    result["battery_low"] = vlow   ? QStringLiteral("是") : QStringLiteral("否");
    result["raw_bits"]    = bitsToHex(bits, 0, static_cast<int>(bits.size()));
    result["bit_count"]   = static_cast<int>(bits.size());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 通用 OOK 固定码（24 / 32 bit）
// ─────────────────────────────────────────────────────────────────────────────
bool CarKeyDecoder::decodeGenericOOK(const std::vector<uint8_t> &bits, QVariantMap &result)
{
    int len = static_cast<int>(bits.size());
    if (len < 24)
        return false;

    // 取前 32 bit（或实际长度）
    int useBits = (len >= 32) ? 32 : 24;
    uint32_t code = bitsToUint(bits, 0, useBits);

    result["protocol"]  = QStringLiteral("通用 OOK 固定码");
    result["code"]      = QString("0x%1").arg(code, useBits / 4, 16, QChar('0')).toUpper();
    result["raw_bits"]  = bitsToHex(bits, 0, len);
    result["bit_count"] = len;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数
// ─────────────────────────────────────────────────────────────────────────────
QString CarKeyDecoder::bitsToHex(const std::vector<uint8_t> &bits, int start, int len)
{
    QString hex;
    for (int i = start; i < start + len && i < static_cast<int>(bits.size()); i += 4) {
        uint8_t nibble = 0;
        for (int j = 0; j < 4 && (i + j) < static_cast<int>(bits.size()); ++j)
            nibble = static_cast<uint8_t>((nibble << 1) | (bits[static_cast<size_t>(i + j)] & 1));
        hex += QString::number(nibble, 16).toUpper();
    }
    return hex;
}

uint32_t CarKeyDecoder::bitsToUint(const std::vector<uint8_t> &bits, int start, int len)
{
    uint32_t val = 0;
    for (int i = 0; i < len && (start + i) < static_cast<int>(bits.size()); ++i)
        val = (val << 1) | (bits[static_cast<size_t>(start + i)] & 1);
    return val;
}

QString CarKeyDecoder::buttonDescription(uint8_t btnBits)
{
    // 常见车钥匙按键位映射（4 bit）
    QStringList btns;
    if (btnBits & 0x08) btns << QStringLiteral("锁车");
    if (btnBits & 0x04) btns << QStringLiteral("开锁");
    if (btnBits & 0x02) btns << QStringLiteral("后备箱");
    if (btnBits & 0x01) btns << QStringLiteral("报警/寻车");
    if (btns.isEmpty())  btns << QStringLiteral("未知");
    return btns.join(" + ");
}