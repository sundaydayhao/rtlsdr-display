#ifndef LORADECODER_H
#define LORADECODER_H

#include <vector>
#include <complex>
#include <QString>
#include <QDateTime>

struct LoraPacket {
    QDateTime timestamp;
    QString data;           // 十六进制字符串
    int rssi;              // 接收信号强度指示 (dBm)
    float snr;             // 信噪比 (dB)
    int frequency;         // 频率 (Hz)
    int bandwidth;         // 带宽 (Hz)
    int spreadingFactor;   // 扩频因子
    bool crcValid;         // CRC 校验是否通过
};

class LoraDecoder
{
public:
    LoraDecoder(int bandwidth, int spreadingFactor);
    ~LoraDecoder();

    // 解码 IQ 样本数据
    std::vector<LoraPacket> decode(const std::vector<std::complex<float>>& samples);

private:
    // 检测前导码
    bool detectPreamble(const std::vector<std::complex<float>>& samples, size_t& position);
    
    // 解调符号
    std::vector<uint8_t> demodulateSymbols(const std::vector<std::complex<float>>& samples, size_t start);
    
    // 解交织
    std::vector<uint8_t> deinterleave(const std::vector<uint8_t>& symbols);
    
    // 解码数据
    std::vector<uint8_t> decodeData(const std::vector<uint8_t>& symbols);
    
    // 计算 RSSI
    int calculateRSSI(const std::vector<std::complex<float>>& samples);
    
    // 计算 SNR
    float calculateSNR(const std::vector<std::complex<float>>& samples);
    
    // 验证 CRC
    bool verifyCRC(const std::vector<uint8_t>& data);

    int m_bandwidth;
    int m_spreadingFactor;
    int m_symbolDuration;
    int m_samplesPerSymbol;
};

#endif // LORADECODER_H