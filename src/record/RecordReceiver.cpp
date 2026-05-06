#include "RecordReceiver.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
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
}

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

#include "RecordReceiver.moc"
