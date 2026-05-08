#include "RecordReceiver.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <vector>

#include <rtl-sdr.h>

namespace {
constexpr quint64 kDefaultFrequency = 100000000ULL;
constexpr quint64 kDefaultSampleRate = 2400000ULL;
constexpr quint64 kDefaultMinFrequency = 24000000ULL;
constexpr quint64 kDefaultMaxFrequency = 1766000000ULL;
constexpr int kReadBufferBytes = 256 * 1024;
constexpr int kFrequencyRetryCount = 5;

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

QString defaultOutputDirectory()
{
    QString directory = QString::fromLocal8Bit(qgetenv("RTLSDR_RECORD_OUTPUT_DIR")).trimmed();
    if (directory.isEmpty()) {
        directory = QString::fromLocal8Bit(qgetenv("RTLSDR_RECORD_DIR")).trimmed();
    }
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    if (directory.isEmpty()) {
        directory = QDir::tempPath();
    }
    return QDir(directory).filePath(QStringLiteral("rtlsdr-recordings"));
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
    profile.minFrequencyHz = envUInt64("RTLSDR_RECORD_MIN_FREQ", profile.minFrequencyHz);
    profile.maxFrequencyHz = envUInt64("RTLSDR_RECORD_MAX_FREQ", profile.maxFrequencyHz);
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

// ---- 回放所需 DSP 工具函数 ----
constexpr double kPlaybackPi = 3.14159265358979323846;
constexpr int    kPlaybackFrameIntervalMs = 45;
constexpr int    kPlaybackReadBytes = 64 * 1024;

std::vector<double> playbackHannWindow(int size)
{
    std::vector<double> w(size);
    for (int i = 0; i < size; ++i) {
        w[i] = 0.5 - 0.5 * std::cos(2.0 * kPlaybackPi * i / static_cast<double>(size - 1));
    }
    return w;
}

void playbackFft(std::vector<std::complex<double>> &v)
{
    const int n = static_cast<int>(v.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(v[i], v[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPlaybackPi / static_cast<double>(len);
        const std::complex<double> step(std::cos(ang), std::sin(ang));
        for (int off = 0; off < n; off += len) {
            std::complex<double> w(1.0, 0.0);
            for (int i = 0; i < len / 2; ++i) {
                const std::complex<double> u = v[off + i];
                const std::complex<double> t = v[off + i + len / 2] * w;
                v[off + i]           = u + t;
                v[off + i + len / 2] = u - t;
                w *= step;
            }
        }
    }
}

QVariantList playbackNormalizeSpectrum(const std::vector<double> &db,
                                       double *floor_, double *ceil_)
{
    std::vector<double> sorted = db;
    std::sort(sorted.begin(), sorted.end());
    const double lo = sorted[static_cast<size_t>(sorted.size() * 0.10)];
    const double hi = sorted[static_cast<size_t>(sorted.size() * 0.98)];

    if (!std::isfinite(*floor_) || !std::isfinite(*ceil_)) {
        *floor_ = lo;
        *ceil_  = std::max(hi, lo + 24.0);
    } else {
        *floor_ = *floor_ * 0.94 + lo * 0.06;
        *ceil_  = *ceil_  * 0.90 + std::max(hi, *floor_ + 18.0) * 0.10;
    }

    const double span = std::max(12.0, *ceil_ - *floor_);
    QVariantList out;
    out.reserve(static_cast<int>(db.size()));
    for (double d : db) {
        out.append(std::max(0.0, std::min(1.0, (d - *floor_) / span)));
    }
    return out;
}

// 从文件名解析中心频率和采样率
// 格式：iq_100000000Hz_2400000sps_20240101_120000.iq
void parseIqFilename(const QString &name, double *centerHz, double *sampleRate)
{
    static const QRegularExpression re(
        QStringLiteral("iq_(\\d+)Hz_(\\d+)sps"));
    const auto m = re.match(name);
    if (m.hasMatch()) {
        bool ok1 = false, ok2 = false;
        const double f = m.captured(1).toDouble(&ok1);
        const double r = m.captured(2).toDouble(&ok2);
        if (ok1 && f > 0.0) *centerHz   = f;
        if (ok2 && r > 0.0) *sampleRate = r;
    }
}

// 生成 SigMF 元数据文件（.sigmf-meta）
// SigMF 规范：https://github.com/sigmf/SigMF/blob/sigmf-v1.x/sigmf-spec.md
void writeSigMFMeta(const QString &iqFilePath,
                    double centerHz,
                    double sampleRate,
                    const QString &isoDatetime)
{
    const QFileInfo fi(iqFilePath);
    const QString metaPath = fi.absolutePath() + QLatin1Char('/') +
                             fi.completeBaseName() + QStringLiteral(".sigmf-meta");
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&metaFile);
    out.setCodec("UTF-8");
    out << "{\n"
        << "  \"global\": {\n"
        << "    \"core:datatype\": \"cu8\",\n"
        << "    \"core:sample_rate\": " << static_cast<qint64>(sampleRate) << ",\n"
        << "    \"core:version\": \"1.0.0\",\n"
        << "    \"core:description\": \"RTL-SDR I/Q recording\"\n"
        << "  },\n"
        << "  \"captures\": [\n"
        << "    {\n"
        << "      \"core:sample_start\": 0,\n"
        << "      \"core:frequency\": " << static_cast<qint64>(centerHz) << ",\n"
        << "      \"core:datetime\": \"" << isoDatetime << "\"\n"
        << "    }\n"
        << "  ],\n"
        << "  \"annotations\": []\n"
        << "}\n";
    metaFile.close();
}

// 从 SigMF 元数据文件读取中心频率和采样率
void parseSigMFMeta(const QString &iqFilePath, double *centerHz, double *sampleRate)
{
    const QFileInfo fi(iqFilePath);
    const QString metaPath = fi.absolutePath() + QLatin1Char('/') +
                             fi.completeBaseName() + QStringLiteral(".sigmf-meta");
    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QString text = QString::fromUtf8(metaFile.readAll());
    metaFile.close();

    static const QRegularExpression reSampleRate(
        QStringLiteral("\"core:sample_rate\"\\s*:\\s*(\\d+)"));
    static const QRegularExpression reFrequency(
        QStringLiteral("\"core:frequency\"\\s*:\\s*(\\d+)"));

    auto m = reSampleRate.match(text);
    if (m.hasMatch()) {
        bool ok = false;
        const double r = m.captured(1).toDouble(&ok);
        if (ok && r > 0.0) *sampleRate = r;
    }
    m = reFrequency.match(text);
    if (m.hasMatch()) {
        bool ok = false;
        const double f = m.captured(1).toDouble(&ok);
        if (ok && f > 0.0) *centerHz = f;
    }
}
} // namespace

class RtlRecordWorker : public QThread
{
    Q_OBJECT

public:
    RtlRecordWorker(uint32_t deviceIndex,
                    uint32_t centerFrequency,
                    uint32_t sampleRate,
                    int ppm,
                    int gain,
                    bool manualGain,
                    int directSampling,
                    const QString &filePath,
                    QObject *parent = nullptr)
        : QThread(parent)
        , m_deviceIndex(deviceIndex)
        , m_sampleRate(sampleRate)
        , m_ppm(ppm)
        , m_gain(gain)
        , m_manualGain(manualGain)
        , m_directSampling(directSampling)
        , m_filePath(filePath)
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
    void statsReady(qulonglong bytesWritten, double elapsedSeconds);
    void recordingFinished(qulonglong bytesWritten, double elapsedSeconds, const QString &filePath);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        emit statusReady(QStringLiteral("正在初始化 RTL-SDR I/Q 录制机"));

        const QFileInfo fileInfo(m_filePath);
        const QString directoryPath = fileInfo.absolutePath();
        if (!QDir().mkpath(directoryPath)) {
            emit statusReady(QStringLiteral("创建保存目录失败：%1").arg(directoryPath));
            return;
        }

        QFile output(m_filePath);
        if (!output.open(QIODevice::WriteOnly)) {
            emit statusReady(QStringLiteral("打开保存文件失败：%1").arg(m_filePath));
            return;
        }

        const uint32_t deviceCount = rtlsdr_get_device_count();
        emit deviceCountReady(static_cast<int>(deviceCount));
        if (deviceCount == 0) {
            emit statusReady(QStringLiteral("未找到 RTL-SDR 设备"));
            output.close();
            QFile::remove(m_filePath);
            return;
        }

        rtlsdr_dev_t *device = nullptr;
        int result = rtlsdr_open(&device, std::min(m_deviceIndex, deviceCount - 1));
        if (result < 0 || device == nullptr) {
            emit statusReady(rtlError(QStringLiteral("打开 RTL-SDR"), result));
            output.close();
            QFile::remove(m_filePath);
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
            output.close();
            QFile::remove(m_filePath);
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
            output.close();
            QFile::remove(m_filePath);
            return;
        }
        currentFrequency = actualFrequency;
        emit centerFrequencyReady(static_cast<double>(currentFrequency));

        result = rtlsdr_reset_buffer(device);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("重置缓冲区"), result));
            closeDevice();
            output.close();
            QFile::remove(m_filePath);
            return;
        }

        emit statusReady(QStringLiteral("I/Q 录制中：%1 / %2 SPS").arg(mhzLabel(currentFrequency)).arg(m_sampleRate));

        const QString recordingStartIso =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        std::vector<unsigned char> raw(kReadBufferBytes);
        QElapsedTimer elapsedTimer;
        QElapsedTimer reportTimer;
        elapsedTimer.start();
        reportTimer.start();
        quint64 bytesWritten = 0;

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
                emit statusReady(QStringLiteral("I/Q 录制中：已调谐到 %1").arg(mhzLabel(currentFrequency)));
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

            const qint64 written = output.write(reinterpret_cast<const char *>(raw.data()), bytesRead);
            if (written != bytesRead) {
                emit statusReady(QStringLiteral("写入 I/Q 文件失败：%1").arg(m_filePath));
                break;
            }

            bytesWritten += static_cast<quint64>(written);
            if (reportTimer.elapsed() >= 250) {
                emit statsReady(bytesWritten, static_cast<double>(elapsedTimer.elapsed()) / 1000.0);
                reportTimer.restart();
            }
        }

        output.flush();
        output.close();
        closeDevice();

        // 生成 SigMF 元数据文件
        writeSigMFMeta(m_filePath,
                       static_cast<double>(currentFrequency),
                       static_cast<double>(m_sampleRate),
                       recordingStartIso);

        const double elapsedSeconds = static_cast<double>(elapsedTimer.elapsed()) / 1000.0;
        emit statsReady(bytesWritten, elapsedSeconds);
        emit recordingFinished(bytesWritten, elapsedSeconds, m_filePath);
        emit statusReady(QStringLiteral("I/Q 录制已保存：%1").arg(m_filePath));
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
    QString m_filePath;
    std::atomic_bool m_stopRequested {false};
    std::atomic<uint32_t> m_requestedFrequency;
};

RecordReceiver::RecordReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("I/Q 录制未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_tunerName(QStringLiteral("未检测"))
    , m_centerFrequencyHz(static_cast<double>(envUInt64("RTLSDR_RECORD_FREQ", kDefaultFrequency)))
    , m_sampleRateHz(static_cast<double>(envUInt64("RTLSDR_RECORD_SAMPLE_RATE", kDefaultSampleRate)))
    , m_minFrequencyHz(static_cast<double>(envUInt64("RTLSDR_RECORD_MIN_FREQ", kDefaultMinFrequency)))
    , m_maxFrequencyHz(static_cast<double>(envUInt64("RTLSDR_RECORD_MAX_FREQ", kDefaultMaxFrequency)))
{
    if (m_minFrequencyHz > m_maxFrequencyHz) {
        std::swap(m_minFrequencyHz, m_maxFrequencyHz);
    }
    m_sampleRateHz = std::max(250000.0, std::min(3200000.0, m_sampleRateHz));
    m_centerFrequencyHz = clampedFrequency(m_centerFrequencyHz);

    const QString configuredFile = QString::fromLocal8Bit(qgetenv("RTLSDR_RECORD_FILE")).trimmed();
    if (configuredFile.isEmpty()) {
        m_outputFilePath = generatedOutputFilePath();
    } else {
        m_outputFilePath = configuredFile;
        m_customOutputFilePath = true;
    }
}

RecordReceiver::~RecordReceiver()
{
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(2000);
    }
}

bool RecordReceiver::running() const
{
    return m_running;
}

QString RecordReceiver::status() const
{
    return m_status;
}

int RecordReceiver::deviceCount() const
{
    return m_deviceCount;
}

QString RecordReceiver::tunerName() const
{
    return m_tunerName;
}

double RecordReceiver::centerFrequencyHz() const
{
    return m_centerFrequencyHz;
}

double RecordReceiver::sampleRateHz() const
{
    return m_sampleRateHz;
}

double RecordReceiver::minFrequencyHz() const
{
    return m_minFrequencyHz;
}

double RecordReceiver::maxFrequencyHz() const
{
    return m_maxFrequencyHz;
}

QString RecordReceiver::outputFilePath() const
{
    return m_outputFilePath;
}

QString RecordReceiver::activeFilePath() const
{
    return m_activeFilePath;
}

QString RecordReceiver::lastSavedPath() const
{
    return m_lastSavedPath;
}

QString RecordReceiver::savedLocation() const
{
    if (!m_activeFilePath.isEmpty()) {
        return m_activeFilePath;
    }
    if (!m_lastSavedPath.isEmpty()) {
        return m_lastSavedPath;
    }
    return m_outputFilePath;
}

qulonglong RecordReceiver::bytesWritten() const
{
    return m_bytesWritten;
}

double RecordReceiver::elapsedSeconds() const
{
    return m_elapsedSeconds;
}

void RecordReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    const QString filePath = resolvedOutputFilePath();
    if (filePath.trimmed().isEmpty()) {
        setStatus(QStringLiteral("保存路径为空"));
        return;
    }

    setRecordingStats(0, 0.0);
    m_activeFilePath = filePath;
    emit savedLocationChanged();
    setStatus(QStringLiteral("正在启动 I/Q 录制"));

    const uint32_t deviceIndex = static_cast<uint32_t>(std::max(0, envInt("RTLSDR_RECORD_DEVICE", 0)));
    const int ppm = envInt("RTLSDR_RECORD_PPM", 0);
    const QByteArray gainEnv = qgetenv("RTLSDR_RECORD_GAIN");
    bool gainOk = false;
    const int gain = QString::fromLatin1(gainEnv).toInt(&gainOk);
    const int directSampling = std::max(0, std::min(2, envInt("RTLSDR_RECORD_DIRECT_SAMPLING", 0)));

    m_worker = new RtlRecordWorker(deviceIndex,
                                   clampToUInt32(m_centerFrequencyHz),
                                   clampToUInt32(m_sampleRateHz),
                                   ppm,
                                   gain,
                                   gainOk,
                                   directSampling,
                                   filePath);
    connect(m_worker, &RtlRecordWorker::statusReady, this, &RecordReceiver::setStatus);
    connect(m_worker, &RtlRecordWorker::deviceCountReady, this, &RecordReceiver::setDeviceCount);
    connect(m_worker, &RtlRecordWorker::tunerInfoReady, this, &RecordReceiver::setTunerInfo);
    connect(m_worker, &RtlRecordWorker::centerFrequencyReady, this, &RecordReceiver::setCenterFrequencyFromWorker);
    connect(m_worker, &RtlRecordWorker::statsReady, this, &RecordReceiver::setRecordingStats);
    connect(m_worker, &RtlRecordWorker::recordingFinished, this, &RecordReceiver::finishRecording);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        if (!m_activeFilePath.isEmpty()) {
            m_activeFilePath.clear();
            emit savedLocationChanged();
        }
        setRunning(false);
    });

    setRunning(true);
    m_worker->start();
}

void RecordReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在停止 I/Q 录制"));
    m_worker->requestStop();
}

void RecordReceiver::tuneMHz(double mhz)
{
    setCenterFrequencyHz(mhz * 1000000.0);
}

void RecordReceiver::stepMHz(double deltaMhz)
{
    setCenterFrequencyHz(m_centerFrequencyHz + deltaMhz * 1000000.0);
}

void RecordReceiver::resetOutputFilePath()
{
    if (m_worker != nullptr) {
        setStatus(QStringLiteral("录制中不能重置保存路径"));
        return;
    }

    m_customOutputFilePath = false;
    m_outputFilePath = generatedOutputFilePath();
    emit outputFilePathChanged();
    emit savedLocationChanged();
}

void RecordReceiver::setCenterFrequencyHz(double frequencyHz)
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

void RecordReceiver::setSampleRateHz(double sampleRateHz)
{
    if (!std::isfinite(sampleRateHz)) {
        return;
    }

    if (m_worker != nullptr) {
        setStatus(QStringLiteral("录制中不能修改采样率"));
        return;
    }

    sampleRateHz = std::max(250000.0, std::min(3200000.0, sampleRateHz));
    if (qFuzzyCompare(m_sampleRateHz, sampleRateHz)) {
        return;
    }

    m_sampleRateHz = sampleRateHz;
    emit sampleRateHzChanged();
    if (!m_customOutputFilePath) {
        m_outputFilePath = generatedOutputFilePath();
        emit outputFilePathChanged();
        emit savedLocationChanged();
    }
}

void RecordReceiver::setOutputFilePath(const QString &filePath)
{
    if (m_worker != nullptr) {
        setStatus(QStringLiteral("录制中不能修改保存路径"));
        return;
    }

    const QString trimmed = filePath.trimmed();
    const bool custom = !trimmed.isEmpty();
    const QString nextPath = custom ? trimmed : generatedOutputFilePath();
    if (m_outputFilePath == nextPath && m_customOutputFilePath == custom) {
        return;
    }

    m_outputFilePath = nextPath;
    m_customOutputFilePath = custom;
    emit outputFilePathChanged();
    emit savedLocationChanged();
}

void RecordReceiver::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void RecordReceiver::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

void RecordReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) {
        return;
    }

    m_deviceCount = count;
    emit deviceCountChanged();
}

void RecordReceiver::setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz)
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

void RecordReceiver::setCenterFrequencyFromWorker(double frequencyHz)
{
    frequencyHz = clampedFrequency(frequencyHz);
    if (qFuzzyCompare(m_centerFrequencyHz, frequencyHz)) {
        return;
    }

    m_centerFrequencyHz = frequencyHz;
    emit centerFrequencyHzChanged();
}

void RecordReceiver::setRecordingStats(qulonglong bytesWritten, double elapsedSeconds)
{
    if (m_bytesWritten == bytesWritten && qFuzzyCompare(m_elapsedSeconds, elapsedSeconds)) {
        return;
    }

    m_bytesWritten = bytesWritten;
    m_elapsedSeconds = elapsedSeconds;
    emit recordingStatsChanged();
}

void RecordReceiver::finishRecording(qulonglong bytesWritten, double elapsedSeconds, const QString &filePath)
{
    setRecordingStats(bytesWritten, elapsedSeconds);
    m_lastSavedPath = filePath;
    m_activeFilePath.clear();
    emit savedLocationChanged();
}

double RecordReceiver::clampedFrequency(double frequencyHz) const
{
    return std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, frequencyHz));
}

bool RecordReceiver::frequencyInRange(double frequencyHz) const
{
    return frequencyHz >= m_minFrequencyHz && frequencyHz <= m_maxFrequencyHz;
}

QString RecordReceiver::generatedOutputFilePath() const
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString filename = QStringLiteral("iq_%1Hz_%2sps_%3.iq")
                                 .arg(clampToUInt32(m_centerFrequencyHz))
                                 .arg(clampToUInt32(m_sampleRateHz))
                                 .arg(timestamp);
    return QDir(defaultOutputDirectory()).filePath(filename);
}

QString RecordReceiver::resolvedOutputFilePath()
{
    if (!m_customOutputFilePath) {
        m_outputFilePath = generatedOutputFilePath();
        emit outputFilePathChanged();
        emit savedLocationChanged();
    }

    QString path = m_outputFilePath.trimmed();
    if (path.isEmpty()) {
        path = generatedOutputFilePath();
    }

    QFileInfo info(path);
    if (path.endsWith(QLatin1Char('/')) || (info.exists() && info.isDir())) {
        path = QDir(path).filePath(QFileInfo(generatedOutputFilePath()).fileName());
    }

    return uniqueFilePath(path);
}

QString RecordReceiver::uniqueFilePath(const QString &filePath) const
{
    QFileInfo info(filePath);
    const QString directory = info.absolutePath();
    QString completeBaseName = info.completeBaseName();
    if (completeBaseName.isEmpty()) {
        completeBaseName = QFileInfo(generatedOutputFilePath()).completeBaseName();
    }
    QString suffix = info.suffix();
    if (suffix.isEmpty()) {
        suffix = QStringLiteral("iq");
    }

    QString candidate = QDir(directory).filePath(completeBaseName + QStringLiteral(".") + suffix);
    int index = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = QDir(directory).filePath(
            QStringLiteral("%1-%2.%3")
                .arg(completeBaseName)
                .arg(index, 3, 10, QLatin1Char('0'))
                .arg(suffix));
        ++index;
    }
    return candidate;
}

// ---------------------------------------------------------------------------
// 回放 Worker
// ---------------------------------------------------------------------------
class RtlPlaybackWorker : public QThread
{
    Q_OBJECT

public:
    RtlPlaybackWorker(const QString &filePath,
                      double centerFrequencyHz,
                      double sampleRateHz,
                      int    fftSize,
                      QObject *parent = nullptr)
        : QThread(parent)
        , m_filePath(filePath)
        , m_centerHz(centerFrequencyHz)
        , m_sampleRate(sampleRateHz)
        , m_fftSize(fftSize)
    {}

    void requestStop() { m_stopRequested.store(true); }

    // 跳转到指定进度（0.0 ~ 1.0），线程安全
    void requestSeek(double progress)
    {
        // 存储为万分之一精度的整数，避免 atomic<double> 的平台兼容问题
        const int val = static_cast<int>(std::max(0.0, std::min(1.0, progress)) * 10000.0);
        m_seekPermille.store(val);
    }

    // 设置回放速度（0.25 ~ 8.0），线程安全
    void setSpeed(double speed)
    {
        const int val = static_cast<int>(std::max(0.25, std::min(8.0, speed)) * 100.0);
        m_speedPercent.store(val);
    }

signals:
    void statusReady(const QString &status);
    void fileInfoReady(double centerHz, double sampleRate, double durationSeconds);
    void progressReady(double progress, double elapsedSeconds);
    void spectrumReady(const QVariantList &levels, double leftHz, double rightHz);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        m_seekPermille.store(-1);

        QFile file(m_filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit statusReady(QStringLiteral("无法打开文件：%1").arg(m_filePath));
            return;
        }

        const qint64 fileSize = file.size();
        if (fileSize < static_cast<qint64>(m_fftSize * 2)) {
            emit statusReady(QStringLiteral("文件太小，无法回放：%1").arg(m_filePath));
            return;
        }

        // 优先从 SigMF 元数据文件读取频率和采样率，其次从文件名解析
        double centerHz   = m_centerHz;
        double sampleRate = m_sampleRate;
        parseSigMFMeta(m_filePath, &centerHz, &sampleRate);
        parseIqFilename(QFileInfo(m_filePath).fileName(), &centerHz, &sampleRate);

        const double durationSeconds =
            static_cast<double>(fileSize) / (sampleRate * 2.0);
        emit fileInfoReady(centerHz, sampleRate, durationSeconds);
        emit statusReady(
            QStringLiteral("回放：%1 / %2 SPS / %3 秒")
                .arg(mhzLabel(centerHz))
                .arg(static_cast<quint64>(sampleRate))
                .arg(durationSeconds, 0, 'f', 1));

        const std::vector<double>         window = playbackHannWindow(m_fftSize);
        std::vector<std::complex<double>> samples(m_fftSize);
        std::vector<double>               specDb(m_fftSize);
        double displayFloor   = std::numeric_limits<double>::quiet_NaN();
        double displayCeiling = std::numeric_limits<double>::quiet_NaN();

        QElapsedTimer elapsed;
        elapsed.start();

        std::vector<unsigned char> raw(kPlaybackReadBytes);

        while (!m_stopRequested.load()) {
            // 处理跳转请求
            const int seekVal = m_seekPermille.exchange(-1);
            if (seekVal >= 0) {
                const double progress = static_cast<double>(seekVal) / 10000.0;
                qint64 seekPos = static_cast<qint64>(progress * static_cast<double>(fileSize));
                // 对齐到样本边界（每个样本 2 字节：I + Q）
                seekPos = (seekPos / 2) * 2;
                seekPos = std::max(qint64(0), std::min(fileSize - 2, seekPos));
                file.seek(seekPos);
                // 重置频谱显示范围
                displayFloor   = std::numeric_limits<double>::quiet_NaN();
                displayCeiling = std::numeric_limits<double>::quiet_NaN();
            }

            const int toRead = static_cast<int>(raw.size());
            const qint64 n   = file.read(reinterpret_cast<char *>(raw.data()), toRead);

            if (n < m_fftSize * 2) {
                // 文件末尾 — 循环回到开头
                file.seek(0);
                emit statusReady(QStringLiteral("回放循环：%1").arg(mhzLabel(centerHz)));
                continue;
            }

            // 进度
            const double progress = std::min(1.0,
                static_cast<double>(file.pos()) / static_cast<double>(fileSize));
            emit progressReady(progress,
                               static_cast<double>(elapsed.elapsed()) / 1000.0);

            // 取最后 fftSize 个样本做 FFT
            const int startOff = static_cast<int>(n) - m_fftSize * 2;
            for (int i = 0; i < m_fftSize; ++i) {
                const int    off = startOff + i * 2;
                const double ip  = (static_cast<double>(raw[off])     - 127.5) / 128.0;
                const double qp  = (static_cast<double>(raw[off + 1]) - 127.5) / 128.0;
                samples[i] = std::complex<double>(ip, qp) * window[i];
            }

            playbackFft(samples);

            const double scale = 1.0 / static_cast<double>(m_fftSize * m_fftSize);
            for (int i = 0; i < m_fftSize; ++i) {
                const int    si    = (i + m_fftSize / 2) % m_fftSize;
                double       power = std::norm(samples[si]) * scale;
                // 修复 DC 尖峰
                if (i == m_fftSize / 2) {
                    const int li = (si - 1 + m_fftSize) % m_fftSize;
                    const int ri = (si + 1) % m_fftSize;
                    power = (std::norm(samples[li]) + std::norm(samples[ri])) * scale / 2.0;
                }
                specDb[i] = 10.0 * std::log10(power + 1.0e-12);
            }

            const QVariantList levels =
                playbackNormalizeSpectrum(specDb, &displayFloor, &displayCeiling);
            const double halfSpan = sampleRate / 2.0;
            emit spectrumReady(levels, centerHz - halfSpan, centerHz + halfSpan);

            // 根据速度调整帧间隔（速度越快，间隔越短）
            const double speed = static_cast<double>(m_speedPercent.load()) / 100.0;
            const int sleepMs = static_cast<int>(
                static_cast<double>(kPlaybackFrameIntervalMs) / std::max(0.01, speed));
            QThread::msleep(static_cast<unsigned long>(std::max(1, sleepMs)));
        }

        file.close();
        emit statusReady(QStringLiteral("回放已停止"));
    }

private:
    QString            m_filePath;
    double             m_centerHz;
    double             m_sampleRate;
    int                m_fftSize;
    std::atomic_bool   m_stopRequested{false};
    std::atomic<int>   m_seekPermille{-1};   // -1 = 无跳转请求；0~10000 = 进度万分比
    std::atomic<int>   m_speedPercent{100};  // 100 = 1x 速度
};

// ---------------------------------------------------------------------------
// RecordReceiver — 回放方法
// ---------------------------------------------------------------------------
bool RecordReceiver::playbackRunning() const { return m_playbackRunning; }
QString RecordReceiver::playbackFilePath() const { return m_playbackFilePath; }
double RecordReceiver::playbackProgress() const { return m_playbackProgress; }
double RecordReceiver::playbackElapsedSeconds() const { return m_playbackElapsedSeconds; }
double RecordReceiver::playbackDurationSeconds() const { return m_playbackDurationSeconds; }
double RecordReceiver::playbackCenterFrequencyHz() const { return m_playbackCenterFrequencyHz; }
double RecordReceiver::playbackSampleRateHz() const { return m_playbackSampleRateHz; }
int RecordReceiver::fftSize() const { return kFftSize; }
double RecordReceiver::playbackSpeed() const { return m_playbackSpeed; }

void RecordReceiver::setPlaybackFilePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (m_playbackFilePath == trimmed) return;
    m_playbackFilePath = trimmed;
    emit playbackFilePathChanged();
}

void RecordReceiver::startPlayback()
{
    if (m_playbackWorker != nullptr) return;
    if (m_running) {
        setStatus(QStringLiteral("请先停止录制再启动回放"));
        return;
    }
    if (m_playbackFilePath.isEmpty()) {
        setStatus(QStringLiteral("请先设置回放文件路径"));
        return;
    }
    if (!QFileInfo::exists(m_playbackFilePath)) {
        setStatus(QStringLiteral("回放文件不存在：%1").arg(m_playbackFilePath));
        return;
    }

    m_playbackProgress       = 0.0;
    m_playbackElapsedSeconds = 0.0;
    emit playbackProgressChanged();

    m_playbackWorker = new RtlPlaybackWorker(m_playbackFilePath,
                                              m_centerFrequencyHz,
                                              m_sampleRateHz,
                                              kFftSize);
    connect(m_playbackWorker, &RtlPlaybackWorker::statusReady,
            this, &RecordReceiver::setStatus);
    connect(m_playbackWorker, &RtlPlaybackWorker::fileInfoReady,
            this, &RecordReceiver::setPlaybackFileInfo);
    connect(m_playbackWorker, &RtlPlaybackWorker::progressReady,
            this, &RecordReceiver::setPlaybackProgress);
    connect(m_playbackWorker, &RtlPlaybackWorker::spectrumReady,
            this, &RecordReceiver::spectrumReady);
    connect(m_playbackWorker, &QThread::finished, this, [this]() {
        m_playbackWorker->deleteLater();
        m_playbackWorker = nullptr;
        setPlaybackRunning(false);
    });

    // 应用当前速度设置
    m_playbackWorker->setSpeed(m_playbackSpeed);

    setPlaybackRunning(true);
    m_playbackWorker->start();
}

void RecordReceiver::stopPlayback()
{
    if (m_playbackWorker == nullptr) return;
    setStatus(QStringLiteral("正在停止回放"));
    m_playbackWorker->requestStop();
}

void RecordReceiver::seekPlayback(double progress)
{
    if (m_playbackWorker != nullptr) {
        m_playbackWorker->requestSeek(progress);
    }
}

void RecordReceiver::setPlaybackSpeed(double speed)
{
    speed = std::max(0.25, std::min(8.0, speed));
    if (qFuzzyCompare(m_playbackSpeed, speed)) return;
    m_playbackSpeed = speed;
    if (m_playbackWorker != nullptr) {
        m_playbackWorker->setSpeed(speed);
    }
    emit playbackSpeedChanged();
}

void RecordReceiver::setPlaybackRunning(bool running)
{
    if (m_playbackRunning == running) return;
    m_playbackRunning = running;
    emit playbackRunningChanged();
}

void RecordReceiver::setPlaybackProgress(double progress, double elapsedSeconds)
{
    m_playbackProgress       = progress;
    m_playbackElapsedSeconds = elapsedSeconds;
    emit playbackProgressChanged();
}

void RecordReceiver::setPlaybackFileInfo(double centerHz, double sampleRate, double durationSeconds)
{
    m_playbackCenterFrequencyHz = centerHz;
    m_playbackSampleRateHz      = sampleRate;
    m_playbackDurationSeconds   = durationSeconds;
    emit playbackFileInfoChanged();
}

#include "RecordReceiver.moc"
