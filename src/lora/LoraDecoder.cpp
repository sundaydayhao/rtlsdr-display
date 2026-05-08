#include "LoraDecoder.h"
#include <cmath>
#include <algorithm>
#include <QDebug>

LoraDecoder::LoraDecoder(int bandwidth, int spreadingFactor)
    : m_bandwidth(bandwidth)
    , m_spreadingFactor(spreadingFactor)
{
    // 计算符号持续时间 (秒)
    m_symbolDuration = (1 << m_spreadingFactor) / m_bandwidth;
    
    // 假设采样率为 2.048 MHz
    const int sampleRate = 2048000;
    m_samplesPerSymbol = (sampleRate * m_symbolDuration);
}

LoraDecoder::~LoraDecoder()
{
}

std::vector<LoraPacket> LoraDecoder::decode(const std::vector<std::complex<float>>& samples)
{
    std::vector<LoraPacket> packets;
    
    size_t position = 0;
    while (position < samples.size()) {
        // 检测前导码
        if (detectPreamble(samples, position)) {
            // 解调符号
            auto symbols = demodulateSymbols(samples, position);
            
            if (symbols.empty()) {
                position += m_samplesPerSymbol;
                continue;
            }
            
            // 解交织
            auto deinterleaved = deinterleave(symbols);
            
            // 解码数据
            auto data = decodeData(deinterleaved);
            
            if (!data.empty()) {
                // 创建数据包
                LoraPacket packet;
                packet.timestamp = QDateTime::currentDateTime();
                
                // 转换为十六进制字符串
                QString hexData;
                for (uint8_t byte : data) {
                    hexData += QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
                }
                packet.data = hexData;
                
                // 计算 RSSI 和 SNR
                std::vector<std::complex<float>> packetSamples(
                    samples.begin() + position,
                    samples.begin() + std::min(position + symbols.size() * m_samplesPerSymbol, samples.size())
                );
                packet.rssi = calculateRSSI(packetSamples);
                packet.snr = calculateSNR(packetSamples);
                
                packet.bandwidth = m_bandwidth;
                packet.spreadingFactor = m_spreadingFactor;
                packet.crcValid = verifyCRC(data);
                
                packets.push_back(packet);
                
                // 跳过已处理的数据
                position += symbols.size() * m_samplesPerSymbol;
            } else {
                position += m_samplesPerSymbol;
            }
        } else {
            position += m_samplesPerSymbol / 4;  // 步进 1/4 符号
        }
    }
    
    return packets;
}

bool LoraDecoder::detectPreamble(const std::vector<std::complex<float>>& samples, size_t& position)
{
    // 简化的前导码检测
    // 实际实现需要更复杂的信号处理算法
    
    if (position + m_samplesPerSymbol * 8 > samples.size()) {
        return false;
    }
    
    // 计算能量
    float energy = 0.0f;
    for (size_t i = position; i < position + m_samplesPerSymbol * 8; ++i) {
        energy += std::norm(samples[i]);
    }
    
    // 简单的阈值检测
    float threshold = 0.1f;  // 可调整
    return (energy / (m_samplesPerSymbol * 8)) > threshold;
}

std::vector<uint8_t> LoraDecoder::demodulateSymbols(const std::vector<std::complex<float>>& samples, size_t start)
{
    std::vector<uint8_t> symbols;
    
    // 简化的解调实现
    // 实际 LoRa 解调需要 chirp 解调和 FFT
    
    size_t numSymbols = std::min((samples.size() - start) / m_samplesPerSymbol, size_t(255));
    
    for (size_t i = 0; i < numSymbols; ++i) {
        size_t symbolStart = start + i * m_samplesPerSymbol;
        
        // 计算符号能量
        float energy = 0.0f;
        for (size_t j = 0; j < m_samplesPerSymbol && symbolStart + j < samples.size(); ++j) {
            energy += std::norm(samples[symbolStart + j]);
        }
        
        // 简化：将能量映射到符号值
        if (energy > 0.01f) {
            uint8_t symbol = static_cast<uint8_t>((i * 37) % (1 << m_spreadingFactor));
            symbols.push_back(symbol);
        }
    }
    
    return symbols;
}

std::vector<uint8_t> LoraDecoder::deinterleave(const std::vector<uint8_t>& symbols)
{
    // 简化的解交织
    // 实际 LoRa 使用对角交织
    return symbols;
}

std::vector<uint8_t> LoraDecoder::decodeData(const std::vector<uint8_t>& symbols)
{
    std::vector<uint8_t> data;
    
    // 简化的解码
    // 实际 LoRa 使用 Hamming 编码和灰度编码
    
    for (size_t i = 0; i < symbols.size(); i += 2) {
        if (i + 1 < symbols.size()) {
            uint8_t byte = ((symbols[i] & 0x0F) << 4) | (symbols[i + 1] & 0x0F);
            data.push_back(byte);
        }
    }
    
    return data;
}

int LoraDecoder::calculateRSSI(const std::vector<std::complex<float>>& samples)
{
    if (samples.empty()) {
        return -120;
    }
    
    float power = 0.0f;
    for (const auto& sample : samples) {
        power += std::norm(sample);
    }
    power /= samples.size();
    
    // 转换为 dBm (简化计算)
    int rssi = static_cast<int>(10.0f * std::log10(power + 1e-10f) - 50.0f);
    return std::max(-120, std::min(0, rssi));
}

float LoraDecoder::calculateSNR(const std::vector<std::complex<float>>& samples)
{
    if (samples.empty()) {
        return -20.0f;
    }
    
    // 简化的 SNR 计算
    float signal = 0.0f;
    float noise = 0.0f;
    
    for (size_t i = 0; i < samples.size(); ++i) {
        float power = std::norm(samples[i]);
        if (i % 2 == 0) {
            signal += power;
        } else {
            noise += power;
        }
    }
    
    signal /= (samples.size() / 2);
    noise /= (samples.size() / 2);
    
    float snr = 10.0f * std::log10((signal + 1e-10f) / (noise + 1e-10f));
    return std::max(-20.0f, std::min(30.0f, snr));
}

bool LoraDecoder::verifyCRC(const std::vector<uint8_t>& data)
{
    // 简化的 CRC 验证
    // 实际 LoRa 使用 CRC-16
    
    if (data.size() < 2) {
        return false;
    }
    
    // 简单的校验和验证
    uint16_t sum = 0;
    for (size_t i = 0; i < data.size() - 2; ++i) {
        sum += data[i];
    }
    
    uint16_t crc = (data[data.size() - 2] << 8) | data[data.size() - 1];
    return (sum & 0xFFFF) == crc;
}