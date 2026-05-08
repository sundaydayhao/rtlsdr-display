#include "LoraReceiver.h"
#include "LoraDecoder.h"
#include "LoraPacketModel.h"
#include <QDebug>
#include <cstring>

LoraReceiver::LoraReceiver(QObject *parent)
    : QObject(parent)
    , m_device(nullptr)
    , m_isRunning(false)
    , m_frequency(433920000)  // 默认 433.92 MHz
    , m_bandwidth(125000)     // 默认 125 kHz
    , m_spreadingFactor(7)    // 默认 SF7
    , m_band("433")
    , m_packetModel(new LoraPacketModel(this))
{
}

LoraReceiver::~LoraReceiver()
{
    stop();
}

void LoraReceiver::start()
{
    if (m_isRunning) {
        qWarning() << "LoraReceiver already running";
        return;
    }

    if (!initializeDevice()) {
        emit errorOccurred("Failed to initialize RTL-SDR device");
        return;
    }

    m_isRunning = true;
    emit isRunningChanged();

    m_thread = std::make_unique<QThread>();
    QObject::connect(m_thread.get(), &QThread::started, this, [this]() {
        receiveLoop();
    });
    
    moveToThread(m_thread.get());
    m_thread->start();

    qDebug() << "LoraReceiver started at" << m_frequency << "Hz, BW:" << m_bandwidth << "Hz, SF:" << m_spreadingFactor;
}

void LoraReceiver::stop()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    emit isRunningChanged();

    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }

    cleanupDevice();
    qDebug() << "LoraReceiver stopped";
}

void LoraReceiver::clearPackets()
{
    m_packetModel->clear();
}

void LoraReceiver::setFrequency(int freq)
{
    if (m_frequency != freq) {
        m_frequency = freq;
        emit frequencyChanged();
        
        if (m_device && m_isRunning) {
            rtlsdr_set_center_freq(m_device, m_frequency);
        }
    }
}

void LoraReceiver::setBandwidth(int bw)
{
    if (m_bandwidth != bw) {
        m_bandwidth = bw;
        emit bandwidthChanged();
    }
}

void LoraReceiver::setSpreadingFactor(int sf)
{
    if (m_spreadingFactor != sf && sf >= 7 && sf <= 12) {
        m_spreadingFactor = sf;
        emit spreadingFactorChanged();
    }
}

void LoraReceiver::setBand(const QString& band)
{
    if (m_band != band) {
        m_band = band;
        emit bandChanged();
        
        // 根据频段设置默认频率
        if (band == "433") {
            setFrequency(433920000);  // 433.92 MHz
        } else if (band == "868") {
            setFrequency(868100000);  // 868.1 MHz
        } else if (band == "915") {
            setFrequency(915000000);  // 915 MHz
        }
    }
}

bool LoraReceiver::initializeDevice()
{
    int device_count = rtlsdr_get_device_count();
    if (device_count == 0) {
        qWarning() << "No RTL-SDR devices found";
        return false;
    }

    int ret = rtlsdr_open(&m_device, 0);
    if (ret < 0) {
        qWarning() << "Failed to open RTL-SDR device";
        return false;
    }

    // 设置采样率
    rtlsdr_set_sample_rate(m_device, SAMPLE_RATE);
    
    // 设置中心频率
    rtlsdr_set_center_freq(m_device, m_frequency);
    
    // 设置增益模式为自动
    rtlsdr_set_tuner_gain_mode(m_device, 0);
    
    // 重置缓冲区
    rtlsdr_reset_buffer(m_device);

    return true;
}

void LoraReceiver::cleanupDevice()
{
    if (m_device) {
        rtlsdr_close(m_device);
        m_device = nullptr;
    }
}

void LoraReceiver::receiveLoop()
{
    uint8_t buffer[BUFFER_SIZE];
    int n_read;

    while (m_isRunning) {
        int ret = rtlsdr_read_sync(m_device, buffer, BUFFER_SIZE, &n_read);
        if (ret < 0) {
            qWarning() << "RTL-SDR read failed";
            break;
        }

        if (n_read > 0) {
            processBuffer(buffer, n_read);
        }
    }
}

void LoraReceiver::processBuffer(const uint8_t* buffer, uint32_t len)
{
    // 使用 LoraDecoder 解码数据
    LoraDecoder decoder(m_bandwidth, m_spreadingFactor);
    
    // 将 IQ 数据转换为复数样本
    std::vector<std::complex<float>> samples;
    samples.reserve(len / 2);
    
    for (uint32_t i = 0; i < len; i += 2) {
        float i_val = (buffer[i] - 127.5f) / 127.5f;
        float q_val = (buffer[i + 1] - 127.5f) / 127.5f;
        samples.emplace_back(i_val, q_val);
    }
    
    // 解码 LoRa 数据包
    auto packets = decoder.decode(samples);
    
    for (const auto& packet : packets) {
        // 添加到模型
        m_packetModel->addPacket(
            packet.timestamp,
            packet.data,
            packet.rssi,
            packet.snr,
            m_frequency,
            m_bandwidth,
            m_spreadingFactor
        );
        
        // 发送信号
        emit packetReceived(packet.data, packet.rssi, packet.snr);
    }
}