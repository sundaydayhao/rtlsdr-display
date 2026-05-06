#include "SensorReceiver.h"

#include "SensorDecoder.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QThread>
#include <QVariantList>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <rtl-sdr.h>

namespace {
constexpr quint64 kDefaultFrequency = 433920000ULL;
constexpr quint64 kDefaultSampleRate = 250000ULL;
constexpr quint64 kDefaultMinFrequency = 24000000ULL;
constexpr quint64 kDefaultMaxFrequency = 1766000000ULL;
constexpr int kReadBufferBytes = 16 * 1024;
constexpr int kFrequencyRetryCount = 5;
constexpr int kFrameResetUs = 12000;
constexpr int kMinPulseUs = 40;
constexpr int kMaxPulseUs = 30000;
constexpr int kMaxFramePulses = 900;

int envInt(const char *name, int fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok ? value : fallback;
}

quint64 envUInt64(const char *name, quint64 fallback)
{
    const QByteArray value = qgetenv(name);
    if (value.isEmpty()) {
        return fallback;
    }

    bool ok = false;
    const quint64 parsed = QString::fromLatin1(value).toULongLong(&ok);
    return ok ? parsed : fallback;
}

uint32_t clampToUInt32(double value)
{
    if (value <= 1.0) {
        return 1U;
    }
    if (value >= static_cast<double>(std::numeric_limits<uint32_t>::max())) {
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(std::llround(value));
}

QString rtlError(const QString &operation, int code)
{
    return QStringLiteral("%1 失败，错误码 %2").arg(operation).arg(code);
}

QString mhzLabel(double hz)
{
    return QStringLiteral("%1 MHz").arg(hz / 1000000.0, 0, 'f', 3);
}

struct TunerProfile {
    QString name;
    quint64 minFrequencyHz = kDefaultMinFrequency;
    quint64 maxFrequencyHz = kDefaultMaxFrequency;
};

TunerProfile profileFor(enum rtlsdr_tuner tuner)
{
    switch (tuner) {
    case RTLSDR_TUNER_E4000:
        return {QStringLiteral("Elonics E4000"), 52000000ULL, 2200000000ULL};
    case RTLSDR_TUNER_FC0012:
        return {QStringLiteral("Fitipower FC0012"), 22000000ULL, 948600000ULL};
    case RTLSDR_TUNER_FC0013:
        return {QStringLiteral("Fitipower FC0013"), 22000000ULL, 1100000000ULL};
    case RTLSDR_TUNER_FC2580:
        return {QStringLiteral("FCI FC2580"), 146000000ULL, 924000000ULL};
    case RTLSDR_TUNER_R820T:
        return {QStringLiteral("Rafael R820T/R820T2"), 24000000ULL, 1766000000ULL};
    case RTLSDR_TUNER_R828D:
        return {QStringLiteral("Rafael R828D/R860"), 24000000ULL, 1766000000ULL};
    case RTLSDR_TUNER_UNKNOWN:
    default:
        return {QStringLiteral("未知 tuner"), kDefaultMinFrequency, kDefaultMaxFrequency};
    }
}

TunerProfile withConfiguredRange(TunerProfile profile)
{
    profile.minFrequencyHz = envUInt64("RTLSDR_SENSOR_MIN_FREQ", profile.minFrequencyHz);
    profile.maxFrequencyHz = envUInt64("RTLSDR_SENSOR_MAX_FREQ", profile.maxFrequencyHz);
    if (profile.minFrequencyHz > profile.maxFrequencyHz) {
        std::swap(profile.minFrequencyHz, profile.maxFrequencyHz);
    }
    return profile;
}

bool tuneWithRetry(rtlsdr_dev_t *device, uint32_t frequency, uint32_t *actualFrequency, QString *error)
{
    for (int attempt = 1; attempt <= kFrequencyRetryCount; ++attempt) {
        const int result = rtlsdr_set_center_freq(device, frequency);
        if (result >= 0) {
            const uint32_t tuned = rtlsdr_get_center_freq(device);
            if (tuned > 0) {
                if (actualFrequency != nullptr) {
                    *actualFrequency = tuned;
                }
                return true;
            }
        }

        QThread::msleep(120);
    }

    if (error != nullptr) {
        *error = QStringLiteral("设置中心频率失败：%1").arg(mhzLabel(frequency));
    }
    return false;
}
}

class RtlSensorWorker : public QThread
{
    Q_OBJECT

public:
    RtlSensorWorker(uint32_t deviceIndex,
                    uint32_t centerFrequency,
                    uint32_t sampleRate,
                    int ppm,
                    int gain,
                    bool manualGain,
                    int directSampling,
                    QObject *parent = nullptr)
        : QThread(parent)
        , m_deviceIndex(deviceIndex)
        , m_sampleRate(sampleRate)
        , m_ppm(ppm)
        , m_gain(gain)
        , m_manualGain(manualGain)
        , m_directSampling(directSampling)
        , m_requestedFrequency(centerFrequency)
    {
    }

    void requestStop()
    {
        m_stopRequested.store(true);
    }

    void requestTune(uint32_t frequency)
    {
        m_requestedFrequency.store(frequency);
    }

signals:
    void statusReady(const QString &status);
    void deviceCountReady(int count);
    void tunerInfoReady(const QString &name, double minFrequencyHz, double maxFrequencyHz);
    void centerFrequencyReady(double frequencyHz);
    void countersReady(qulonglong totalFrames, qulonglong decodedMessages);
    void signalLevelReady(double level);
    void eventsReady(const QVariantList &events);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        emit statusReady(QStringLiteral("正在初始化 433/315 MHz 传感器接收机"));

        const uint32_t deviceCount = rtlsdr_get_device_count();
        emit deviceCountReady(static_cast<int>(deviceCount));
        if (deviceCount == 0) {
            emit statusReady(QStringLiteral("未找到 RTL-SDR 设备"));
            return;
        }

        rtlsdr_dev_t *device = nullptr;
        int result = rtlsdr_open(&device, std::min(m_deviceIndex, deviceCount - 1));
        if (result < 0 || device == nullptr) {
            emit statusReady(rtlError(QStringLiteral("打开 RTL-SDR"), result));
            return;
        }

        const auto closeDevice = [&]() {
            rtlsdr_close(device);
            device = nullptr;
        };

        TunerProfile profile = withConfiguredRange(profileFor(rtlsdr_get_tuner_type(device)));
        emit tunerInfoReady(profile.name,
                            static_cast<double>(profile.minFrequencyHz),
                            static_cast<double>(profile.maxFrequencyHz));

        result = rtlsdr_set_sample_rate(device, m_sampleRate);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("设置采样率"), result));
            closeDevice();
            return;
        }

        rtlsdr_set_freq_correction(device, m_ppm);
        rtlsdr_set_agc_mode(device, 1);
        rtlsdr_set_direct_sampling(device, m_directSampling);
        rtlsdr_set_offset_tuning(device, 0);
        rtlsdr_set_tuner_bandwidth(device, m_sampleRate);
        if (m_manualGain) {
            rtlsdr_set_tuner_gain_mode(device, 1);
            rtlsdr_set_tuner_gain(device, m_gain);
        } else {
            rtlsdr_set_tuner_gain_mode(device, 0);
        }

        uint32_t currentFrequency = boundedRequestedFrequency(profile);
        m_requestedFrequency.store(currentFrequency);
        uint32_t actualFrequency = currentFrequency;
        QString tuneError;
        if (!tuneWithRetry(device, currentFrequency, &actualFrequency, &tuneError)) {
            emit statusReady(tuneError);
            closeDevice();
            return;
        }
        currentFrequency = actualFrequency;
        emit centerFrequencyReady(static_cast<double>(currentFrequency));

        result = rtlsdr_reset_buffer(device);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("重置缓冲区"), result));
            closeDevice();
            return;
        }

        emit statusReady(QStringLiteral("传感器解码中：%1 / %2 SPS / OOK-ASK")
                             .arg(mhzLabel(currentFrequency))
                             .arg(m_sampleRate));

        SensorDecoder decoder;
        std::vector<unsigned char> raw(kReadBufferBytes);
        std::vector<SensorPulse> frame;
        frame.reserve(256);

        bool hasState = false;
        bool currentHigh = false;
        int currentSamples = 0;
        double noiseFloor = 0.0;
        double signalPeak = 0.0;
        double currentSignalLevel = 0.0;
        quint64 totalFrames = 0;
        quint64 decodedMessages = 0;

        QElapsedTimer reportTimer;
        reportTimer.start();

        const double usPerSample = 1000000.0 / static_cast<double>(m_sampleRate);

        const auto decodeFrame = [&]() {
            int highPulseCount = 0;
            for (const SensorPulse &pulse : frame) {
                if (pulse.high) {
                    ++highPulseCount;
                }
            }
            if (highPulseCount < 8) {
                frame.clear();
                return;
            }

            ++totalFrames;
            const QList<QVariantMap> decoded = decoder.decode(frame,
                                                              static_cast<double>(currentFrequency),
                                                              currentSignalLevel);
            frame.clear();

            if (!decoded.isEmpty()) {
                QVariantList events;
                events.reserve(decoded.size());
                for (const QVariantMap &event : decoded) {
                    events.append(event);
                }
                decodedMessages += static_cast<quint64>(decoded.size());
                emit eventsReady(events);
            }
            emit countersReady(totalFrames, decodedMessages);
        };

        const auto appendPulse = [&](bool high, int samples) {
            if (samples <= 0) {
                return;
            }

            const int durationUs = static_cast<int>(std::llround(samples * usPerSample));
            if (!high && durationUs >= kFrameResetUs) {
                decodeFrame();
                return;
            }
            if (durationUs < kMinPulseUs || durationUs > kMaxPulseUs) {
                return;
            }
            if (frame.empty() && !high) {
                return;
            }

            frame.push_back({high, durationUs});
            if (static_cast<int>(frame.size()) >= kMaxFramePulses) {
                decodeFrame();
            }
        };

        while (!m_stopRequested.load()) {
            const uint32_t requested = boundedRequestedFrequency(profile);
            if (requested != currentFrequency) {
                if (!tuneWithRetry(device, requested, &actualFrequency, &tuneError)) {
                    emit statusReady(tuneError);
                    QThread::msleep(150);
                    continue;
                }

                currentFrequency = actualFrequency;
                m_requestedFrequency.store(currentFrequency);
                emit centerFrequencyReady(static_cast<double>(currentFrequency));
                rtlsdr_reset_buffer(device);
                frame.clear();
                hasState = false;
                emit statusReady(QStringLiteral("传感器解码中：已调谐到 %1").arg(mhzLabel(currentFrequency)));
            }

            int bytesRead = 0;
            result = rtlsdr_read_sync(device, raw.data(), static_cast<int>(raw.size()), &bytesRead);
            if (result < 0) {
                emit statusReady(rtlError(QStringLiteral("读取 RTL-SDR 数据"), result));
                break;
            }
            if (bytesRead <= 0) {
                continue;
            }

            const int usableBytes = bytesRead & ~1;
            for (int offset = 0; offset < usableBytes; offset += 2) {
                const double inPhase = static_cast<double>(static_cast<int>(raw[offset]) - 127);
                const double quadrature = static_cast<double>(static_cast<int>(raw[offset + 1]) - 127);
                const double power = inPhase * inPhase + quadrature * quadrature;

                if (!hasState) {
                    noiseFloor = power;
                    signalPeak = power + 1.0;
                    currentHigh = false;
                    currentSamples = 1;
                    hasState = true;
                    continue;
                }

                if (!currentHigh || power < noiseFloor) {
                    noiseFloor = noiseFloor * 0.9992 + power * 0.0008;
                }
                if (power > signalPeak) {
                    signalPeak = signalPeak * 0.94 + power * 0.06;
                } else {
                    signalPeak = signalPeak * 0.9994 + power * 0.0006;
                }

                const double spread = std::max(1.0, signalPeak - noiseFloor);
                const double highThreshold = noiseFloor + spread * 0.35;
                const double lowThreshold = noiseFloor + spread * 0.18;
                bool nextHigh = currentHigh ? power > lowThreshold : power > highThreshold;
                if (spread < 500.0) {
                    nextHigh = false;
                }

                if (nextHigh == currentHigh) {
                    ++currentSamples;
                } else {
                    appendPulse(currentHigh, currentSamples);
                    currentHigh = nextHigh;
                    currentSamples = 1;
                }

                currentSignalLevel = std::max(0.0, std::min(1.0, spread / 18000.0));
            }

            if (reportTimer.elapsed() >= 250) {
                emit signalLevelReady(currentSignalLevel);
                reportTimer.restart();
            }
        }

        if (hasState) {
            appendPulse(currentHigh, currentSamples);
            decodeFrame();
        }

        closeDevice();
        emit signalLevelReady(0.0);
        emit statusReady(QStringLiteral("传感器解码已停止"));
    }

private:
    uint32_t boundedRequestedFrequency(const TunerProfile &profile) const
    {
        const uint32_t requested = m_requestedFrequency.load();
        const double bounded = std::max(static_cast<double>(profile.minFrequencyHz),
                                        std::min(static_cast<double>(profile.maxFrequencyHz),
                                                 static_cast<double>(requested)));
        return clampToUInt32(bounded);
    }

    uint32_t m_deviceIndex = 0;
    uint32_t m_sampleRate = static_cast<uint32_t>(kDefaultSampleRate);
    int m_ppm = 0;
    int m_gain = 0;
    bool m_manualGain = false;
    int m_directSampling = 0;
    std::atomic_bool m_stopRequested {false};
    std::atomic<uint32_t> m_requestedFrequency;
};

SensorReceiver::SensorReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("传感器解码未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_tunerName(QStringLiteral("未检测"))
    , m_centerFrequencyHz(static_cast<double>(envUInt64("RTLSDR_SENSOR_FREQ", kDefaultFrequency)))
    , m_sampleRateHz(static_cast<double>(envUInt64("RTLSDR_SENSOR_SAMPLE_RATE", kDefaultSampleRate)))
    , m_minFrequencyHz(static_cast<double>(envUInt64("RTLSDR_SENSOR_MIN_FREQ", kDefaultMinFrequency)))
    , m_maxFrequencyHz(static_cast<double>(envUInt64("RTLSDR_SENSOR_MAX_FREQ", kDefaultMaxFrequency)))
{
    if (m_minFrequencyHz > m_maxFrequencyHz) {
        std::swap(m_minFrequencyHz, m_maxFrequencyHz);
    }
    m_sampleRateHz = std::max(200000.0, std::min(1200000.0, m_sampleRateHz));
    m_centerFrequencyHz = clampedFrequency(m_centerFrequencyHz);

    m_pruneTimer.setInterval(15000);
    connect(&m_pruneTimer, &QTimer::timeout, this, [this]() {
        const int before = m_eventModel.eventCount();
        m_eventModel.pruneStale(180);
        if (before != m_eventModel.eventCount()) {
            emit sensorCountChanged();
        }
    });
    m_pruneTimer.start();
}

SensorReceiver::~SensorReceiver()
{
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(2000);
    }
}

bool SensorReceiver::running() const
{
    return m_running;
}

QString SensorReceiver::status() const
{
    return m_status;
}

int SensorReceiver::deviceCount() const
{
    return m_deviceCount;
}

QString SensorReceiver::tunerName() const
{
    return m_tunerName;
}

double SensorReceiver::centerFrequencyHz() const
{
    return m_centerFrequencyHz;
}

double SensorReceiver::sampleRateHz() const
{
    return m_sampleRateHz;
}

double SensorReceiver::minFrequencyHz() const
{
    return m_minFrequencyHz;
}

double SensorReceiver::maxFrequencyHz() const
{
    return m_maxFrequencyHz;
}

QString SensorReceiver::bandLabel() const
{
    const double mhz = m_centerFrequencyHz / 1000000.0;
    if (std::abs(mhz - 433.92) < 2.0) {
        return QStringLiteral("433 MHz");
    }
    if (std::abs(mhz - 315.0) < 2.0) {
        return QStringLiteral("315 MHz");
    }
    if (std::abs(mhz - 303.4) < 2.0) {
        return QStringLiteral("303 MHz");
    }
    return QStringLiteral("%1 MHz").arg(mhz, 0, 'f', 3);
}

qulonglong SensorReceiver::totalFrames() const
{
    return m_totalFrames;
}

qulonglong SensorReceiver::decodedMessages() const
{
    return m_decodedMessages;
}

int SensorReceiver::sensorCount() const
{
    return m_eventModel.eventCount();
}

double SensorReceiver::signalLevel() const
{
    return m_signalLevel;
}

QObject *SensorReceiver::eventModel()
{
    return &m_eventModel;
}

void SensorReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在启动传感器解码"));
    const uint32_t deviceIndex = static_cast<uint32_t>(std::max(0, envInt("RTLSDR_SENSOR_DEVICE", 0)));
    const int ppm = envInt("RTLSDR_SENSOR_PPM", 0);
    const QByteArray gainEnv = qgetenv("RTLSDR_SENSOR_GAIN");
    bool gainOk = false;
    const int gain = QString::fromLatin1(gainEnv).toInt(&gainOk);
    const int directSampling = std::max(0, std::min(2, envInt("RTLSDR_SENSOR_DIRECT_SAMPLING", 0)));

    m_worker = new RtlSensorWorker(deviceIndex,
                                   clampToUInt32(m_centerFrequencyHz),
                                   clampToUInt32(m_sampleRateHz),
                                   ppm,
                                   gain,
                                   gainOk,
                                   directSampling);
    connect(m_worker, &RtlSensorWorker::statusReady, this, &SensorReceiver::setStatus);
    connect(m_worker, &RtlSensorWorker::deviceCountReady, this, &SensorReceiver::setDeviceCount);
    connect(m_worker, &RtlSensorWorker::tunerInfoReady, this, &SensorReceiver::setTunerInfo);
    connect(m_worker, &RtlSensorWorker::centerFrequencyReady, this, &SensorReceiver::setCenterFrequencyFromWorker);
    connect(m_worker, &RtlSensorWorker::countersReady, this, &SensorReceiver::setCounters);
    connect(m_worker, &RtlSensorWorker::signalLevelReady, this, &SensorReceiver::setSignalLevel);
    connect(m_worker, &RtlSensorWorker::eventsReady, this, &SensorReceiver::handleEvents);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        setRunning(false);
    });

    setRunning(true);
    m_worker->start();
}

void SensorReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }
    setStatus(QStringLiteral("正在停止传感器解码"));
    m_worker->requestStop();
}

void SensorReceiver::tuneMHz(double mhz)
{
    setCenterFrequencyHz(mhz * 1000000.0);
}

void SensorReceiver::stepMHz(double deltaMhz)
{
    setCenterFrequencyHz(m_centerFrequencyHz + deltaMhz * 1000000.0);
}

void SensorReceiver::setBand(const QString &band)
{
    const QString normalized = band.trimmed().toLower();
    if (normalized.startsWith(QStringLiteral("315"))) {
        setCenterFrequencyHz(315000000.0);
        return;
    }
    if (normalized.startsWith(QStringLiteral("303"))) {
        setCenterFrequencyHz(303400000.0);
        return;
    }
    if (normalized.startsWith(QStringLiteral("433"))) {
        setCenterFrequencyHz(433920000.0);
    }
}

void SensorReceiver::clearEvents()
{
    if (m_eventModel.eventCount() == 0) {
        return;
    }
    m_eventModel.clear();
    emit sensorCountChanged();
}

void SensorReceiver::setCenterFrequencyHz(double frequencyHz)
{
    if (!std::isfinite(frequencyHz)) {
        return;
    }

    if (!frequencyInRange(frequencyHz)) {
        setStatus(QStringLiteral("频率超出范围：%1 - %2")
                      .arg(mhzLabel(m_minFrequencyHz), mhzLabel(m_maxFrequencyHz)));
        frequencyHz = clampedFrequency(frequencyHz);
    }

    if (qFuzzyCompare(m_centerFrequencyHz, frequencyHz)) {
        return;
    }

    m_centerFrequencyHz = frequencyHz;
    emit centerFrequencyHzChanged();

    if (m_worker != nullptr) {
        m_worker->requestTune(clampToUInt32(m_centerFrequencyHz));
    }
}

void SensorReceiver::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_running = running;
    emit runningChanged();
}

void SensorReceiver::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void SensorReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) {
        return;
    }
    m_deviceCount = count;
    emit deviceCountChanged();
}

void SensorReceiver::setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz)
{
    const bool changed = m_tunerName != name
        || !qFuzzyCompare(m_minFrequencyHz, minFrequencyHz)
        || !qFuzzyCompare(m_maxFrequencyHz, maxFrequencyHz);
    if (!changed) {
        return;
    }

    m_tunerName = name;
    m_minFrequencyHz = minFrequencyHz;
    m_maxFrequencyHz = maxFrequencyHz;
    emit tunerInfoChanged();

    const double bounded = clampedFrequency(m_centerFrequencyHz);
    if (!qFuzzyCompare(bounded, m_centerFrequencyHz)) {
        setCenterFrequencyFromWorker(bounded);
    }
}

void SensorReceiver::setCenterFrequencyFromWorker(double frequencyHz)
{
    frequencyHz = clampedFrequency(frequencyHz);
    if (qFuzzyCompare(m_centerFrequencyHz, frequencyHz)) {
        return;
    }
    m_centerFrequencyHz = frequencyHz;
    emit centerFrequencyHzChanged();
}

void SensorReceiver::setCounters(qulonglong totalFrames, qulonglong decodedMessages)
{
    if (m_totalFrames == totalFrames && m_decodedMessages == decodedMessages) {
        return;
    }
    m_totalFrames = totalFrames;
    m_decodedMessages = decodedMessages;
    emit countersChanged();
}

void SensorReceiver::setSignalLevel(double level)
{
    level = std::max(0.0, std::min(1.0, level));
    if (qFuzzyCompare(m_signalLevel, level)) {
        return;
    }
    m_signalLevel = level;
    emit signalLevelChanged();
}

void SensorReceiver::handleEvents(const QVariantList &events)
{
    const int before = m_eventModel.eventCount();
    for (const QVariant &event : events) {
        m_eventModel.updateEvent(event.toMap());
    }
    if (before != m_eventModel.eventCount()) {
        emit sensorCountChanged();
    }
}

double SensorReceiver::clampedFrequency(double frequencyHz) const
{
    return std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, frequencyHz));
}

bool SensorReceiver::frequencyInRange(double frequencyHz) const
{
    return frequencyHz >= m_minFrequencyHz && frequencyHz <= m_maxFrequencyHz;
}

#include "SensorReceiver.moc"
