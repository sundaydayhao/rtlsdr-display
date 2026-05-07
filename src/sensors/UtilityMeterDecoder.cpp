#include "UtilityMeterDecoder.h"
#include <QDateTime>
#include <cstring>

UtilityMeterDecoder::UtilityMeterDecoder(QObject *parent)
    : SceneDecoder(parent)
{
}

bool UtilityMeterDecoder::decode(const std::vector<uint8_t> &bits, double freq)
{
    if (bits.size() < 64)
        return false;

    QVariantMap result;
    result[QStringLiteral("scene")]     = sceneName();
    result[QStringLiteral("frequency")] = QString("%1 MHz").arg(freq / 1e6, 0, 'f', 3);
    result[QStringLiteral("timestamp")] = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    if (decodeERT_SCM(bits, result)) {
        emit decoded(result);
        return true;
    }
    if (decodeWMBus(bits, result)) {
        emit decoded(result);
        return true;
    }
    if (decodeDomestic433(bits, result)) {
        emit decoded(result);
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// ERT SCM (Standard Consumption Message)  76 bit
// 帧结构：
//   前导 21 bit 0 + 同步 0x16A3 (16 bit) + 数据 37 bit + CRC 2 bit
//   数据：ERT Type 2 bit | Physical Tamper 2 bit | Phy Layer 2 bit |
//          Meter ID 26 bit | Consumption 24 bit | Tamper Count 2 bit |
//          Backflow 1 bit | Last Consumption 18 bit
// ─────────────────────────────────────────────────────────────────────────────
bool UtilityMeterDecoder::decodeERT_SCM(const std::vector<uint8_t> &bits, QVariantMap &result)
{
    if (bits.size() < 76)
        return false;

    // 搜索同步字 0x16A3 = 0001 0110 1010 0011
    const uint8_t sync[] = {0,0,0,1, 0,1,1,0, 1,0,1,0, 0,0,1,1};
    int syncPos = -1;
    for (int i = 0; i <= static_cast<int>(bits.size()) - 16; ++i) {
        bool match = true;
        for (int j = 0; j < 16; ++j) {
            if (bits[static_cast<size_t>(i + j)] != sync[j]) { match = false; break; }
        }
        if (match) { syncPos = i; break; }
    }
    if (syncPos < 0)
        return false;

    int dataStart = syncPos + 16;
    if (dataStart + 56 > static_cast<int>(bits.size()))
        return false;

    uint32_t ertType     = bitsToUint(bits, dataStart,      2);
    uint32_t physTamper  = bitsToUint(bits, dataStart + 2,  2);
    uint32_t phyLayer    = bitsToUint(bits, dataStart + 4,  2);
    uint32_t meterId     = bitsToUint(bits, dataStart + 6,  26);
    uint32_t consumption = bitsToUint(bits, dataStart + 32, 24);

    result[QStringLiteral("protocol")]    = QStringLiteral("ERT SCM");
    result[QStringLiteral("meter_id")]    = QString::number(meterId);
    result[QStringLiteral("ert_type")]    = QString::number(ertType);
    result[QStringLiteral("consumption")] = QString::number(consumption);
    result[QStringLiteral("phy_tamper")]  = physTamper ? QStringLiteral("告警") : QStringLiteral("正常");
    result[QStringLiteral("phy_layer")]   = QString::number(phyLayer);
    result[QStringLiteral("meter_type")]  = meterTypeStr(static_cast<uint8_t>(ertType));
    result[QStringLiteral("raw")]         = bytesToHex(bitsToBytes(bits, 0, static_cast<int>(bits.size()) / 8));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Wireless M-Bus Mode T / C
// 帧头：L(1B) + C(1B) + M(2B) + A(6B) + CI(1B) + 数据
// ─────────────────────────────────────────────────────────────────────────────
bool UtilityMeterDecoder::decodeWMBus(const std::vector<uint8_t> &bits, QVariantMap &result)
{
    if (bits.size() < 80)
        return false;

    // 尝试曼彻斯特解码
    std::vector<uint8_t> decoded;
    if (!manchesterDecode(bits, decoded))
        return false;

    if (decoded.size() < 10)
        return false;

    uint8_t  L  = decoded[0];   // 帧长度（不含 L 字节本身）
    uint8_t  C  = decoded[1];   // 控制字段
    uint16_t M  = static_cast<uint16_t>((decoded[3] << 8) | decoded[2]); // 制造商 ID (little-endian)
    // 地址 6 字节：decoded[4..9]
    uint8_t  CI = (decoded.size() > 10) ? decoded[10] : 0;

    // 制造商 ID 解码（3 字母 EN 13757 编码）
    char mfr[4] = {0};
    mfr[0] = static_cast<char>('@' + ((M >> 10) & 0x1F));
    mfr[1] = static_cast<char>('@' + ((M >> 5)  & 0x1F));
    mfr[2] = static_cast<char>('@' + ( M        & 0x1F));

    // 设备地址（BCD 编码，4 字节）
    QString addr;
    for (int i = 4; i < 8 && i < static_cast<int>(decoded.size()); ++i)
        addr += QString("%1").arg(decoded[i], 2, 16, QChar('0')).toUpper();

    // 设备类型
    uint8_t devType = (decoded.size() > 9) ? decoded[9] : 0;
    QString devTypeStr;
    switch (devType) {
    case 0x00: devTypeStr = QStringLiteral("其他"); break;
    case 0x01: devTypeStr = QStringLiteral("油表"); break;
    case 0x02: devTypeStr = QStringLiteral("电表"); break;
    case 0x03: devTypeStr = QStringLiteral("燃气表"); break;
    case 0x04: devTypeStr = QStringLiteral("热量表"); break;
    case 0x06: devTypeStr = QStringLiteral("温水表"); break;
    case 0x07: devTypeStr = QStringLiteral("水表"); break;
    case 0x08: devTypeStr = QStringLiteral("热量分配器"); break;
    case 0x0A: devTypeStr = QStringLiteral("冷水表"); break;
    case 0x0B: devTypeStr = QStringLiteral("热水表"); break;
    case 0x0C: devTypeStr = QStringLiteral("热量表(入口)"); break;
    case 0x0D: devTypeStr = QStringLiteral("热量表(出口)"); break;
    default:   devTypeStr = QString("0x%1").arg(devType, 2, 16, QChar('0')).toUpper(); break;
    }

    result[QStringLiteral("protocol")]    = QStringLiteral("Wireless M-Bus");
    result[QStringLiteral("manufacturer")]= QString::fromLatin1(mfr);
    result[QStringLiteral("meter_id")]    = addr;
    result[QStringLiteral("meter_type")]  = devTypeStr;
    result[QStringLiteral("frame_len")]   = QString::number(L);
    result[QStringLiteral("control")]     = QString("0x%1").arg(C, 2, 16, QChar('0')).toUpper();
    result[QStringLiteral("ci_field")]    = QString("0x%1").arg(CI, 2, 16, QChar('0')).toUpper();
    result[QStringLiteral("raw")]         = bytesToHex(decoded);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 国内 433 MHz 抄表协议（简化帧）
// 帧格式：前导 0xAA 0xAA + 同步 0x2D 0xD4 + 地址 4B + 数据 N B + CRC16
// ─────────────────────────────────────────────────────────────────────────────
bool UtilityMeterDecoder::decodeDomestic433(const std::vector<uint8_t> &bits, QVariantMap &result)
{
    if (bits.size() < 80)
        return false;

    // 转换为字节
    std::vector<uint8_t> bytes = bitsToBytes(bits, 0, static_cast<int>(bits.size()) / 8);
    if (bytes.size() < 10)
        return false;

    // 搜索同步头 0xAA 0xAA 0x2D 0xD4
    int syncPos = -1;
    for (int i = 0; i + 3 < static_cast<int>(bytes.size()); ++i) {
        if (bytes[i] == 0xAA && bytes[i+1] == 0xAA &&
            bytes[i+2] == 0x2D && bytes[i+3] == 0xD4) {
            syncPos = i + 4;
            break;
        }
    }
    if (syncPos < 0 || syncPos + 6 > static_cast<int>(bytes.size()))
        return false;

    // 地址 4 字节（BCD 编码）
    QString addr;
    for (int i = syncPos; i < syncPos + 4; ++i)
        addr += QString("%1").arg(bytes[i], 2, 16, QChar('0')).toUpper();

    // 数据字节
    int dataLen = static_cast<int>(bytes.size()) - syncPos - 4 - 2; // 减去地址和CRC
    if (dataLen < 0) dataLen = 0;

    // CRC 校验
    bool crcOk = false;
    if (syncPos + 4 + dataLen + 2 <= static_cast<int>(bytes.size())) {
        uint16_t calcCrc = crc16(bytes, syncPos, 4 + dataLen);
        uint16_t frameCrc = static_cast<uint16_t>(
            (bytes[syncPos + 4 + dataLen] << 8) | bytes[syncPos + 4 + dataLen + 1]);
        crcOk = (calcCrc == frameCrc);
    }

    // 读数（假设前 4 字节数据为 BCD 编码的读数）
    QString reading;
    for (int i = syncPos + 4; i < syncPos + 4 + std::min(dataLen, 4); ++i)
        reading += QString("%1").arg(bytes[i], 2, 16, QChar('0'));

    result[QStringLiteral("protocol")]  = QStringLiteral("国内433MHz抄表");
    result[QStringLiteral("meter_id")]  = addr;
    result[QStringLiteral("reading")]   = reading;
    result[QStringLiteral("crc_ok")]    = crcOk ? QStringLiteral("通过") : QStringLiteral("失败");
    result[QStringLiteral("raw")]       = bytesToHex(bytes);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数
// ─────────────────────────────────────────────────────────────────────────────
std::vector<uint8_t> UtilityMeterDecoder::bitsToBytes(const std::vector<uint8_t> &bits, int start, int byteCount)
{
    std::vector<uint8_t> bytes;
    for (int i = 0; i < byteCount; ++i) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b) {
            int idx = start + i * 8 + b;
            if (idx < static_cast<int>(bits.size()))
                byte = static_cast<uint8_t>((byte << 1) | (bits[static_cast<size_t>(idx)] & 1));
            else
                byte = static_cast<uint8_t>(byte << 1);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

uint16_t UtilityMeterDecoder::crc16(const std::vector<uint8_t> &data, int start, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = start; i < start + len && i < static_cast<int>(data.size()); ++i) {
        crc ^= static_cast<uint16_t>(data[static_cast<size_t>(i)] << 8);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000)
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

bool UtilityMeterDecoder::manchesterDecode(const std::vector<uint8_t> &bits, std::vector<uint8_t> &decoded)
{
    // 曼彻斯特编码：每两个比特编码一个数据比特
    // 01 -> 0, 10 -> 1
    if (bits.size() < 16)
        return false;

    std::vector<uint8_t> dataBits;
    for (size_t i = 0; i + 1 < bits.size(); i += 2) {
        uint8_t b0 = bits[i] & 1;
        uint8_t b1 = bits[i+1] & 1;
        if (b0 == 0 && b1 == 1)
            dataBits.push_back(0);
        else if (b0 == 1 && b1 == 0)
            dataBits.push_back(1);
        else
            return false; // 无效的曼彻斯特编码
    }

    // 将比特转换为字节
    for (size_t i = 0; i + 7 < dataBits.size(); i += 8) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b)
            byte = static_cast<uint8_t>((byte << 1) | dataBits[i + b]);
        decoded.push_back(byte);
    }
    return !decoded.empty();
}

QString UtilityMeterDecoder::bytesToHex(const std::vector<uint8_t> &bytes)
{
    QString hex;
    for (uint8_t b : bytes)
        hex += QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
    return hex;
}

uint32_t UtilityMeterDecoder::bitsToUint(const std::vector<uint8_t> &bits, int start, int len)
{
    uint32_t val = 0;
    for (int i = 0; i < len && (start + i) < static_cast<int>(bits.size()); ++i)
        val = (val << 1) | (bits[static_cast<size_t>(start + i)] & 1);
    return val;
}

QString UtilityMeterDecoder::meterTypeStr(uint8_t type)
{
    switch (type) {
    case 0: return QStringLiteral("电表");
    case 1: return QStringLiteral("气表");
    case 2: return QStringLiteral("水表");
    case 3: return QStringLiteral("热量表");
    default: return QStringLiteral("未知仪表");
    }
}