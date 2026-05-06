#include "WaterfallReceiver.h"

#include <QElapsedTimer>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include <rtl-sdr.h>

namespace {
constexpr quint64 kDefaultFrequency = 100000000ULL;
constexpr quint64 kDefaultSampleRate = 2400000ULL;
constexpr quint64 kDefaultMinFrequency = 24000000ULL;
constexpr quint64 kDefaultMaxFrequency = 1766000000ULL;
constexpr int kDefaultFftSize = 512;
constexpr int kMinFftSize = 128;
constexpr int kMaxFftSize = 2048;
constexpr int kReadBufferBytes = 64 * 1024;
constexpr int kFrameIntervalMs = 45;
constexpr int kFrequencyRetryCount = 5;
constexpr double kPi = 3.14159265358979323846;

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

int nearestPowerOfTwo(int value)
{
    value = std::max(kMinFftSize, std::min(kMaxFftSize, value));

    int power = 1;
    while (power < value) {
        power <<= 1;
    }

    const int lower = power >> 1;
    if (lower >= kMinFftSize && value - lower < power - value) {
        return lower;
    }
    return power;
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
    profile.minFrequencyHz = envUInt64("RTLSDR_WATERFALL_MIN_FREQ", profile.minFrequencyHz);
    profile.maxFrequencyHz = envUInt64("RTLSDR_WATERFALL_MAX_FREQ", profile.maxFrequencyHz);
    if (profile.minFrequencyHz > profile.maxFrequencyHz) {
        std::swap(profile.minFrequencyHz, profile.maxFrequencyHz);
    }
    return profile;
}

QString mhzLabel(double hz)
{
    return QStringLiteral("%1 MHz").arg(hz / 1000000.0, 0, 'f', 3);
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

std::vector<double> hannWindow(int size)
{
    std::vector<double> window(size);
    if (size <= 1) {
        return window;
    }

    for (int i = 0; i < size; ++i) {
        window[i] = 0.5 - 0.5 * std::cos(2.0 * kPi * i / static_cast<double>(size - 1));
    }
    return window;
}

void fft(std::vector<std::complex<double>> &values)
{
    const int size = static_cast<int>(values.size());
    for (int i = 1, j = 0; i < size; ++i) {
        int bit = size >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;

        if (i < j) {
            std::swap(values[i], values[j]);
        }
    }

    for (int length = 2; length <= size; length <<= 1) {
        const double angle = -2.0 * kPi / static_cast<double>(length);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (int offset = 0; offset < size; offset += length) {
            std::complex<double> weight(1.0, 0.0);
            for (int i = 0; i < length / 2; ++i) {
                const std::complex<double> even = values[offset + i];
                const std::complex<double> odd = values[offset + i + length / 2] * weight;
                values[offset + i] = even + odd;
                values[offset + i + length / 2] = even - odd;
                weight *= step;
            }
        }
    }
}

QVariantList normalizeSpectrum(const std::vector<double> &dbValues, double *displayFloor, double *displayCeiling)
{
    std::vector<double> sorted = dbValues;
    std::sort(sorted.begin(), sorted.end());
    const double low = sorted[static_cast<size_t>(sorted.size() * 0.10)];
    const double high = sorted[static_cast<size_t>(sorted.size() * 0.98)];

    if (!std::isfinite(*displayFloor) || !std::isfinite(*displayCeiling)) {
        *displayFloor = low;
        *displayCeiling = std::max(high, low + 24.0);
    } else {
        *displayFloor = *displayFloor * 0.94 + low * 0.06;
        *displayCeiling = *displayCeiling * 0.90 + std::max(high, *displayFloor + 18.0) * 0.10;
    }

    const double span = std::max(12.0, *displayCeiling - *displayFloor);
    QVariantList levels;
    levels.reserve(static_cast<int>(dbValues.size()));
    for (double db : dbValues) {
        const double normalized = std::max(0.0, std::min(1.0, (db - *displayFloor) / span));
        levels.append(normalized);
    }
    return levels;
}
}

class RtlWaterfallWorker : public QThread
{
    Q_OBJECT

public:
    RtlWaterfallWorker(uint32_t deviceIndex,
                       uint32_t centerFrequency,
                       uint32_t sampleRate,
                       int ppm,
                       int gain,
                       bool manualGain,
                       int directSampling,
                       int fftSize,
                       QObject *parent = nullptr)
        : QThread(parent)
        , m_deviceIndex(deviceIndex)
        , m_sampleRate(sampleRate)
        , m_ppm(ppm)
        , m_gain(gain)
        , m_manualGain(manualGain)
        , m_directSampling(directSampling)
        , m_fftSize(fftSize)
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
    void spectrumReady(const QVariantList &levels, double leftFrequencyHz, double rightFrequencyHz);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        emit statusReady(QStringLiteral("正在初始化 RTL-SDR 瀑布图接收机"));

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
        rtlsdr_set_tuner_bandwidth(device, 0);
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

        emit statusReady(QStringLiteral("瀑布图接收中：%1 / %2 SPS / %3")
                             .arg(mhzLabel(currentFrequency))
                             .arg(m_sampleRate)
                             .arg(profile.name));

        std::vector<unsigned char> raw(kReadBufferBytes);
        std::vector<std::complex<double>> samples(m_fftSize);
        std::vector<double> spectrumDb(m_fftSize);
        const std::vector<double> window = hannWindow(m_fftSize);
        double displayFloor = std::numeric_limits<double>::quiet_NaN();
        double displayCeiling = std::numeric_limits<double>::quiet_NaN();
        QElapsedTimer frameTimer;
        frameTimer.start();

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
                emit statusReady(QStringLiteral("已调谐到 %1").arg(mhzLabel(currentFrequency)));
                frameTimer.restart();
            }

            int bytesRead = 0;
            result = rtlsdr_read_sync(device, raw.data(), static_cast<int>(raw.size()), &bytesRead);
            if (result < 0) {
                emit statusReady(rtlError(QStringLiteral("读取 RTL-SDR 数据"), result));
                break;
            }
            const int availableBytes = bytesRead & ~1;
            if (availableBytes < m_fftSize * 2 || frameTimer.elapsed() < kFrameIntervalMs) {
                continue;
            }
            frameTimer.restart();

            const int start = availableBytes - m_fftSize * 2;
            for (int i = 0; i < m_fftSize; ++i) {
                const int offset = start + i * 2;
                const double inPhase = (static_cast<double>(raw[offset]) - 127.5) / 128.0;
                const double quadrature = (static_cast<double>(raw[offset + 1]) - 127.5) / 128.0;
                samples[i] = std::complex<double>(inPhase, quadrature) * window[i];
            }

            fft(samples);

            const double scale = 1.0 / static_cast<double>(m_fftSize * m_fftSize);
            for (int i = 0; i < m_fftSize; ++i) {
                const int shiftedIndex = (i + m_fftSize / 2) % m_fftSize;
                const double power = std::norm(samples[shiftedIndex]) * scale;
                spectrumDb[i] = 10.0 * std::log10(power + 1.0e-12);
            }

            const QVariantList levels = normalizeSpectrum(spectrumDb, &displayFloor, &displayCeiling);
            const double halfSpan = static_cast<double>(m_sampleRate) / 2.0;
            emit spectrumReady(levels,
                               static_cast<double>(currentFrequency) - halfSpan,
                               static_cast<double>(currentFrequency) + halfSpan);
        }

        closeDevice();
        emit statusReady(QStringLiteral("瀑布图已停止"));
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
    int m_fftSize = kDefaultFftSize;
    std::atomic_bool m_stopRequested {false};
    std::atomic<uint32_t> m_requestedFrequency;
};

WaterfallReceiver::WaterfallReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("瀑布图未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_tunerName(QStringLiteral("未检测"))
    , m_centerFrequencyHz(static_cast<double>(envUInt64("RTLSDR_WATERFALL_FREQ", kDefaultFrequency)))
    , m_sampleRateHz(static_cast<double>(envUInt64("RTLSDR_WATERFALL_SAMPLE_RATE", kDefaultSampleRate)))
    , m_minFrequencyHz(static_cast<double>(envUInt64("RTLSDR_WATERFALL_MIN_FREQ", kDefaultMinFrequency)))
    , m_maxFrequencyHz(static_cast<double>(envUInt64("RTLSDR_WATERFALL_MAX_FREQ", kDefaultMaxFrequency)))
    , m_fftSize(nearestPowerOfTwo(envInt("RTLSDR_WATERFALL_FFT_SIZE", kDefaultFftSize)))
{
    if (m_minFrequencyHz > m_maxFrequencyHz) {
        std::swap(m_minFrequencyHz, m_maxFrequencyHz);
    }
    m_sampleRateHz = std::max(250000.0, std::min(3200000.0, m_sampleRateHz));
    m_centerFrequencyHz = clampedFrequency(m_centerFrequencyHz);
}

WaterfallReceiver::~WaterfallReceiver()
{
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(2000);
    }
}

bool WaterfallReceiver::running() const
{
    return m_running;
}

QString WaterfallReceiver::status() const
{
    return m_status;
}

int WaterfallReceiver::deviceCount() const
{
    return m_deviceCount;
}

QString WaterfallReceiver::tunerName() const
{
    return m_tunerName;
}

double WaterfallReceiver::centerFrequencyHz() const
{
    return m_centerFrequencyHz;
}

double WaterfallReceiver::sampleRateHz() const
{
    return m_sampleRateHz;
}

double WaterfallReceiver::minFrequencyHz() const
{
    return m_minFrequencyHz;
}

double WaterfallReceiver::maxFrequencyHz() const
{
    return m_maxFrequencyHz;
}

int WaterfallReceiver::fftSize() const
{
    return m_fftSize;
}

void WaterfallReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在启动瀑布图接收机"));
    const uint32_t deviceIndex = static_cast<uint32_t>(std::max(0, envInt("RTLSDR_WATERFALL_DEVICE", 0)));
    const int ppm = envInt("RTLSDR_WATERFALL_PPM", 0);
    const QByteArray gainEnv = qgetenv("RTLSDR_WATERFALL_GAIN");
    bool gainOk = false;
    const int gain = QString::fromLatin1(gainEnv).toInt(&gainOk);
    const int directSampling = std::max(0, std::min(2, envInt("RTLSDR_WATERFALL_DIRECT_SAMPLING", 0)));

    m_worker = new RtlWaterfallWorker(deviceIndex,
                                      clampToUInt32(m_centerFrequencyHz),
                                      clampToUInt32(m_sampleRateHz),
                                      ppm,
                                      gain,
                                      gainOk,
                                      directSampling,
                                      m_fftSize);
    connect(m_worker, &RtlWaterfallWorker::statusReady, this, &WaterfallReceiver::setStatus);
    connect(m_worker, &RtlWaterfallWorker::deviceCountReady, this, &WaterfallReceiver::setDeviceCount);
    connect(m_worker, &RtlWaterfallWorker::tunerInfoReady, this, &WaterfallReceiver::setTunerInfo);
    connect(m_worker, &RtlWaterfallWorker::centerFrequencyReady, this, &WaterfallReceiver::setCenterFrequencyFromWorker);
    connect(m_worker, &RtlWaterfallWorker::spectrumReady, this, &WaterfallReceiver::spectrumReady);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        setRunning(false);
    });

    setRunning(true);
    m_worker->start();
}

void WaterfallReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在停止瀑布图接收机"));
    m_worker->requestStop();
}

void WaterfallReceiver::tuneMHz(double mhz)
{
    setCenterFrequencyHz(mhz * 1000000.0);
}

void WaterfallReceiver::stepMHz(double deltaMhz)
{
    setCenterFrequencyHz(m_centerFrequencyHz + deltaMhz * 1000000.0);
}

void WaterfallReceiver::setCenterFrequencyHz(double frequencyHz)
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

void WaterfallReceiver::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void WaterfallReceiver::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

void WaterfallReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) {
        return;
    }

    m_deviceCount = count;
    emit deviceCountChanged();
}

void WaterfallReceiver::setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz)
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

void WaterfallReceiver::setCenterFrequencyFromWorker(double frequencyHz)
{
    frequencyHz = clampedFrequency(frequencyHz);
    if (qFuzzyCompare(m_centerFrequencyHz, frequencyHz)) {
        return;
    }

    m_centerFrequencyHz = frequencyHz;
    emit centerFrequencyHzChanged();
}

double WaterfallReceiver::clampedFrequency(double frequencyHz) const
{
    return std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, frequencyHz));
}

bool WaterfallReceiver::frequencyInRange(double frequencyHz) const
{
    return frequencyHz >= m_minFrequencyHz && frequencyHz <= m_maxFrequencyHz;
}

#include "WaterfallReceiver.moc"
