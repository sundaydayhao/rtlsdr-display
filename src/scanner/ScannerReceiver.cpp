#include "ScannerReceiver.h"

#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

#include <rtl-sdr.h>

namespace {
constexpr quint64 kScanDefaultStart      = 88000000ULL;
constexpr quint64 kScanDefaultStop       = 108000000ULL;
constexpr quint64 kScanDefaultSampleRate = 2400000ULL;
constexpr quint64 kScanDefaultMinFreq    = 24000000ULL;
constexpr quint64 kScanDefaultMaxFreq    = 1766000000ULL;
constexpr int     kScanDefaultFftSize    = 512;
constexpr int     kScanMinFftSize        = 128;
constexpr int     kScanMaxFftSize        = 2048;
constexpr int     kScanReadBufferBytes   = 64 * 1024;
constexpr int     kScanSettleMs          = 50;   // ms to wait after each retune
constexpr double  kPi                    = 3.14159265358979323846;

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
    value = std::max(kScanMinFftSize, std::min(kScanMaxFftSize, value));
    int power = 1;
    while (power < value) {
        power <<= 1;
    }
    const int lower = power >> 1;
    if (lower >= kScanMinFftSize && value - lower < power - value) {
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

QString mhzLabel(double hz)
{
    return QStringLiteral("%1 MHz").arg(hz / 1000000.0, 0, 'f', 3);
}

struct TunerProfile {
    QString name;
    quint64 minFrequencyHz = kScanDefaultMinFreq;
    quint64 maxFrequencyHz = kScanDefaultMaxFreq;
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
        return {QStringLiteral("未知 tuner"), kScanDefaultMinFreq, kScanDefaultMaxFreq};
    }
}

TunerProfile withScannerRange(TunerProfile profile)
{
    profile.minFrequencyHz = envUInt64("RTLSDR_SCANNER_MIN_FREQ", profile.minFrequencyHz);
    profile.maxFrequencyHz = envUInt64("RTLSDR_SCANNER_MAX_FREQ", profile.maxFrequencyHz);
    if (profile.minFrequencyHz > profile.maxFrequencyHz) {
        std::swap(profile.minFrequencyHz, profile.maxFrequencyHz);
    }
    return profile;
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
                const std::complex<double> odd  = values[offset + i + length / 2] * weight;
                values[offset + i]              = even + odd;
                values[offset + i + length / 2] = even - odd;
                weight *= step;
            }
        }
    }
}

QVariantList normalizeSpectrum(const std::vector<double> &dbValues,
                               double *displayFloor,
                               double *displayCeiling)
{
    std::vector<double> sorted = dbValues;
    std::sort(sorted.begin(), sorted.end());
    const double low  = sorted[static_cast<size_t>(sorted.size() * 0.10)];
    const double high = sorted[static_cast<size_t>(sorted.size() * 0.98)];

    if (!std::isfinite(*displayFloor) || !std::isfinite(*displayCeiling)) {
        *displayFloor   = low;
        *displayCeiling = std::max(high, low + 24.0);
    } else {
        *displayFloor   = *displayFloor * 0.92 + low * 0.08;
        *displayCeiling = *displayCeiling * 0.88 + std::max(high, *displayFloor + 18.0) * 0.12;
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
} // namespace

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------
class RtlScanWorker : public QThread
{
    Q_OBJECT

public:
    RtlScanWorker(uint32_t deviceIndex,
                  uint32_t startFreq,
                  uint32_t stopFreq,
                  uint32_t sampleRate,
                  int      ppm,
                  int      gain,
                  bool     manualGain,
                  int      fftSize,
                  QObject *parent = nullptr)
        : QThread(parent)
        , m_deviceIndex(deviceIndex)
        , m_sampleRate(sampleRate)
        , m_ppm(ppm)
        , m_gain(gain)
        , m_manualGain(manualGain)
        , m_fftSize(fftSize)
        , m_requestedStart(startFreq)
        , m_requestedStop(stopFreq)
    {
    }

    void requestStop()
    {
        m_stopRequested.store(true);
    }

    void requestRange(uint32_t start, uint32_t stop)
    {
        m_requestedStart.store(start);
        m_requestedStop.store(stop);
    }

signals:
    void statusReady(const QString &status);
    void deviceCountReady(int count);
    void tunerInfoReady(const QString &name, double minHz, double maxHz);
    void totalBinsReady(int bins);
    void scanLineReady(const QVariantList &powers, double startHz, double stopHz);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        emit statusReady(QStringLiteral("正在初始化频谱扫描接收机"));

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

        TunerProfile profile = withScannerRange(profileFor(rtlsdr_get_tuner_type(device)));
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
        rtlsdr_set_offset_tuning(device, 0);
        rtlsdr_set_tuner_bandwidth(device, 0);
        if (m_manualGain) {
            rtlsdr_set_tuner_gain_mode(device, 1);
            rtlsdr_set_tuner_gain(device, m_gain);
        } else {
            rtlsdr_set_tuner_gain_mode(device, 0);
        }

        std::vector<unsigned char>       raw(kScanReadBufferBytes);
        std::vector<std::complex<double>> samples(m_fftSize);
        const std::vector<double>        window = hannWindow(m_fftSize);
        double displayFloor   = std::numeric_limits<double>::quiet_NaN();
        double displayCeiling = std::numeric_limits<double>::quiet_NaN();

        while (!m_stopRequested.load()) {
            // ---- read current scan range from atomics ----
            uint32_t startFreq = bounded(m_requestedStart.load(), profile);
            uint32_t stopFreq  = bounded(m_requestedStop.load(), profile);
            if (startFreq >= stopFreq) {
                stopFreq = std::min(startFreq + m_sampleRate,
                                    static_cast<uint32_t>(profile.maxFrequencyHz));
            }

            const int numHops  = std::max(1,
                static_cast<int>(std::ceil(
                    static_cast<double>(stopFreq - startFreq) /
                    static_cast<double>(m_sampleRate))));
            const int totalBins = numHops * m_fftSize;
            emit totalBinsReady(totalBins);

            std::vector<double> linePowers;
            linePowers.reserve(static_cast<size_t>(totalBins));

            bool scanOk = true;
            for (int hop = 0; hop < numHops && !m_stopRequested.load(); ++hop) {
                const uint32_t centerFreq =
                    startFreq
                    + static_cast<uint32_t>(hop) * m_sampleRate
                    + m_sampleRate / 2u;

                // Tune
                result = rtlsdr_set_center_freq(device, centerFreq);
                if (result < 0) {
                    emit statusReady(QStringLiteral("调谐失败：%1").arg(mhzLabel(centerFreq)));
                    scanOk = false;
                    break;
                }
                QThread::msleep(kScanSettleMs);

                // Flush and read
                rtlsdr_reset_buffer(device);
                int bytesRead = 0;
                result = rtlsdr_read_sync(device, raw.data(),
                                          static_cast<int>(raw.size()), &bytesRead);
                if (result < 0) {
                    emit statusReady(rtlError(QStringLiteral("读取数据"), result));
                    scanOk = false;
                    break;
                }

                const int availableBytes = bytesRead & ~1;
                if (availableBytes < m_fftSize * 2) {
                    // Not enough data — fill with noise floor
                    for (int i = 0; i < m_fftSize; ++i) {
                        linePowers.push_back(-120.0);
                    }
                    continue;
                }

                // Convert uint8 IQ → windowed complex samples
                const int startOffset = availableBytes - m_fftSize * 2;
                for (int i = 0; i < m_fftSize; ++i) {
                    const int    off = startOffset + i * 2;
                    const double ip  = (static_cast<double>(raw[off])     - 127.5) / 128.0;
                    const double qp  = (static_cast<double>(raw[off + 1]) - 127.5) / 128.0;
                    samples[i] = std::complex<double>(ip, qp) * window[i];
                }

                fft(samples);

                // Power spectrum (FFT-shifted), fix DC spike
                const double scale = 1.0 / static_cast<double>(m_fftSize * m_fftSize);
                for (int i = 0; i < m_fftSize; ++i) {
                    const int shiftedIdx = (i + m_fftSize / 2) % m_fftSize;
                    double power = std::norm(samples[shiftedIdx]) * scale;

                    // Replace DC bin with average of neighbours
                    if (i == m_fftSize / 2) {
                        const int li = (shiftedIdx - 1 + m_fftSize) % m_fftSize;
                        const int ri = (shiftedIdx + 1) % m_fftSize;
                        power = (std::norm(samples[li]) + std::norm(samples[ri])) * scale / 2.0;
                    }

                    linePowers.push_back(10.0 * std::log10(power + 1.0e-12));
                }
            }

            if (!scanOk || m_stopRequested.load()) {
                break;
            }

            // Normalise entire line and emit
            const QVariantList levels =
                normalizeSpectrum(linePowers, &displayFloor, &displayCeiling);

            const double actualStop =
                static_cast<double>(startFreq) +
                static_cast<double>(numHops) * static_cast<double>(m_sampleRate);

            emit scanLineReady(levels,
                               static_cast<double>(startFreq),
                               actualStop);

            emit statusReady(
                QStringLiteral("扫描中：%1 → %2 / %3 跳 / %4 个频点")
                    .arg(mhzLabel(startFreq))
                    .arg(mhzLabel(static_cast<double>(stopFreq)))
                    .arg(numHops)
                    .arg(totalBins));
        }

        closeDevice();
        emit statusReady(QStringLiteral("扫描已停止"));
    }

private:
    uint32_t bounded(uint32_t freq, const TunerProfile &profile) const
    {
        const double lo = static_cast<double>(profile.minFrequencyHz);
        const double hi = static_cast<double>(profile.maxFrequencyHz);
        return clampToUInt32(std::max(lo, std::min(hi, static_cast<double>(freq))));
    }

    uint32_t          m_deviceIndex = 0;
    uint32_t          m_sampleRate;
    int               m_ppm        = 0;
    int               m_gain       = 0;
    bool              m_manualGain = false;
    int               m_fftSize;
    std::atomic_bool  m_stopRequested{false};
    std::atomic<uint32_t> m_requestedStart;
    std::atomic<uint32_t> m_requestedStop;
};

// ---------------------------------------------------------------------------
// ScannerReceiver
// ---------------------------------------------------------------------------
ScannerReceiver::ScannerReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("扫描未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_tunerName(QStringLiteral("未检测"))
    , m_startFrequencyHz(static_cast<double>(
          envUInt64("RTLSDR_SCANNER_START_FREQ", kScanDefaultStart)))
    , m_stopFrequencyHz(static_cast<double>(
          envUInt64("RTLSDR_SCANNER_STOP_FREQ", kScanDefaultStop)))
    , m_sampleRateHz(static_cast<double>(
          envUInt64("RTLSDR_SCANNER_SAMPLE_RATE", kScanDefaultSampleRate)))
    , m_minFrequencyHz(static_cast<double>(
          envUInt64("RTLSDR_SCANNER_MIN_FREQ", kScanDefaultMinFreq)))
    , m_maxFrequencyHz(static_cast<double>(
          envUInt64("RTLSDR_SCANNER_MAX_FREQ", kScanDefaultMaxFreq)))
    , m_fftSize(nearestPowerOfTwo(
          envInt("RTLSDR_SCANNER_FFT_SIZE", kScanDefaultFftSize)))
{
    if (m_minFrequencyHz > m_maxFrequencyHz) {
        std::swap(m_minFrequencyHz, m_maxFrequencyHz);
    }
    m_sampleRateHz = std::max(250000.0, std::min(3200000.0, m_sampleRateHz));
    if (m_startFrequencyHz < m_minFrequencyHz) m_startFrequencyHz = m_minFrequencyHz;
    if (m_stopFrequencyHz  > m_maxFrequencyHz) m_stopFrequencyHz  = m_maxFrequencyHz;
    if (m_startFrequencyHz >= m_stopFrequencyHz) {
        m_stopFrequencyHz = m_startFrequencyHz + m_sampleRateHz;
    }
    updateTotalBins();
}

ScannerReceiver::~ScannerReceiver()
{
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(3000);
    }
}

bool ScannerReceiver::running() const { return m_running; }
QString ScannerReceiver::status() const { return m_status; }
int ScannerReceiver::deviceCount() const { return m_deviceCount; }
QString ScannerReceiver::tunerName() const { return m_tunerName; }
double ScannerReceiver::startFrequencyHz() const { return m_startFrequencyHz; }
double ScannerReceiver::stopFrequencyHz() const { return m_stopFrequencyHz; }
double ScannerReceiver::sampleRateHz() const { return m_sampleRateHz; }
int ScannerReceiver::fftSize() const { return m_fftSize; }
int ScannerReceiver::totalBins() const { return m_totalBins; }
double ScannerReceiver::minFrequencyHz() const { return m_minFrequencyHz; }
double ScannerReceiver::maxFrequencyHz() const { return m_maxFrequencyHz; }

void ScannerReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在启动频谱扫描接收机"));

    const uint32_t deviceIndex =
        static_cast<uint32_t>(std::max(0, envInt("RTLSDR_SCANNER_DEVICE", 0)));
    const int ppm = envInt("RTLSDR_SCANNER_PPM", 0);
    const QByteArray gainEnv = qgetenv("RTLSDR_SCANNER_GAIN");
    bool gainOk = false;
    const int gain = QString::fromLatin1(gainEnv).toInt(&gainOk);

    m_worker = new RtlScanWorker(deviceIndex,
                                  clampToUInt32(m_startFrequencyHz),
                                  clampToUInt32(m_stopFrequencyHz),
                                  clampToUInt32(m_sampleRateHz),
                                  ppm,
                                  gain,
                                  gainOk,
                                  m_fftSize);

    connect(m_worker, &RtlScanWorker::statusReady,
            this, &ScannerReceiver::setStatus);
    connect(m_worker, &RtlScanWorker::deviceCountReady,
            this, &ScannerReceiver::setDeviceCount);
    connect(m_worker, &RtlScanWorker::tunerInfoReady,
            this, &ScannerReceiver::setTunerInfo);
    connect(m_worker, &RtlScanWorker::totalBinsReady,
            this, [this](int bins) {
                if (m_totalBins == bins) return;
                m_totalBins = bins;
                emit totalBinsChanged();
            });
    connect(m_worker, &RtlScanWorker::scanLineReady,
            this, &ScannerReceiver::scanLineReady);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        setRunning(false);
    });

    setRunning(true);
    m_worker->start();
}

void ScannerReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }
    setStatus(QStringLiteral("正在停止扫描接收机"));
    m_worker->requestStop();
}

void ScannerReceiver::setStartFrequencyHz(double hz)
{
    hz = std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, hz));
    if (hz >= m_stopFrequencyHz) {
        hz = m_stopFrequencyHz - m_sampleRateHz;
    }
    if (hz < m_minFrequencyHz) {
        hz = m_minFrequencyHz;
    }
    if (qFuzzyCompare(m_startFrequencyHz, hz)) {
        return;
    }
    m_startFrequencyHz = hz;
    emit startFrequencyHzChanged();
    updateTotalBins();
    if (m_worker != nullptr) {
        m_worker->requestRange(clampToUInt32(m_startFrequencyHz),
                               clampToUInt32(m_stopFrequencyHz));
    }
}

void ScannerReceiver::setStopFrequencyHz(double hz)
{
    hz = std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, hz));
    if (hz <= m_startFrequencyHz) {
        hz = m_startFrequencyHz + m_sampleRateHz;
    }
    if (hz > m_maxFrequencyHz) {
        hz = m_maxFrequencyHz;
    }
    if (qFuzzyCompare(m_stopFrequencyHz, hz)) {
        return;
    }
    m_stopFrequencyHz = hz;
    emit stopFrequencyHzChanged();
    updateTotalBins();
    if (m_worker != nullptr) {
        m_worker->requestRange(clampToUInt32(m_startFrequencyHz),
                               clampToUInt32(m_stopFrequencyHz));
    }
}

void ScannerReceiver::setRunning(bool running)
{
    if (m_running == running) return;
    m_running = running;
    emit runningChanged();
}

void ScannerReceiver::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit statusChanged();
}

void ScannerReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) return;
    m_deviceCount = count;
    emit deviceCountChanged();
}

void ScannerReceiver::setTunerInfo(const QString &name, double minHz, double maxHz)
{
    const bool changed = m_tunerName != name
        || !qFuzzyCompare(m_minFrequencyHz, minHz)
        || !qFuzzyCompare(m_maxFrequencyHz, maxHz);
    if (!changed) return;

    m_tunerName       = name;
    m_minFrequencyHz  = minHz;
    m_maxFrequencyHz  = maxHz;
    emit tunerInfoChanged();
}

void ScannerReceiver::updateTotalBins()
{
    const int numHops = std::max(1,
        static_cast<int>(std::ceil(
            (m_stopFrequencyHz - m_startFrequencyHz) / m_sampleRateHz)));
    const int bins = numHops * m_fftSize;
    if (m_totalBins == bins) return;
    m_totalBins = bins;
    emit totalBinsChanged();
}

#include "ScannerReceiver.moc"