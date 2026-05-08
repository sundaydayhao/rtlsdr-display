#ifndef LORARECEIVER_H
#define LORARECEIVER_H

#include <QObject>
#include <QThread>
#include <rtl-sdr.h>
#include <atomic>
#include <memory>

class LoraPacketModel;

class LoraReceiver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(int bandwidth READ bandwidth WRITE setBandwidth NOTIFY bandwidthChanged)
    Q_PROPERTY(int spreadingFactor READ spreadingFactor WRITE setSpreadingFactor NOTIFY spreadingFactorChanged)
    Q_PROPERTY(QString band READ band WRITE setBand NOTIFY bandChanged)
    Q_PROPERTY(LoraPacketModel* packetModel READ packetModel CONSTANT)

public:
    explicit LoraReceiver(QObject *parent = nullptr);
    ~LoraReceiver();

    bool isRunning() const { return m_isRunning; }
    int frequency() const { return m_frequency; }
    int bandwidth() const { return m_bandwidth; }
    int spreadingFactor() const { return m_spreadingFactor; }
    QString band() const { return m_band; }
    LoraPacketModel* packetModel() const { return m_packetModel; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clearPackets();

public slots:
    void setFrequency(int freq);
    void setBandwidth(int bw);
    void setSpreadingFactor(int sf);
    void setBand(const QString& band);

signals:
    void isRunningChanged();
    void frequencyChanged();
    void bandwidthChanged();
    void spreadingFactorChanged();
    void bandChanged();
    void packetReceived(const QString& data, int rssi, float snr);
    void errorOccurred(const QString& error);

private:
    void receiveLoop();
    void processBuffer(const uint8_t* buffer, uint32_t len);
    bool initializeDevice();
    void cleanupDevice();

    rtlsdr_dev_t* m_device;
    std::atomic<bool> m_isRunning;
    std::unique_ptr<QThread> m_thread;
    
    int m_frequency;        // 中心频率 (Hz)
    int m_bandwidth;        // 带宽 (Hz)
    int m_spreadingFactor;  // 扩频因子 (7-12)
    QString m_band;         // 频段标识 (433/868/915)
    
    LoraPacketModel* m_packetModel;
    
    static constexpr uint32_t SAMPLE_RATE = 2048000;  // 2.048 MHz
    static constexpr uint32_t BUFFER_SIZE = 262144;   // 256 KB
};

#endif // LORARECEIVER_H