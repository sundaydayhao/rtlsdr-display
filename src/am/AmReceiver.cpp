#include "AmReceiver.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QProcess>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <rtl-sdr.h>

namespace {
constexpr quint64 kDefaultTunerFrequency = 118000000ULL;
constexpr quint64 kDefaultDirectSamplingFrequency = 7200000ULL;
constexpr quint64 kDefaultSampleRate = 240000ULL;
constexpr quint64 kDefaultAudioRate = 48000ULL;
constexpr quint64 kDefaultTunerMinFrequency = 24000000ULL;
constexpr quint64 kDefaultTunerMaxFrequency = 1766000000ULL;
constexpr quint64 kDefaultDirectSamplingMinFrequency = 100000ULL;
constexpr quint64 kDefaultDirectSamplingMaxFrequency = 30000000ULL;
constexpr int kReadBufferBytes = 64 * 1024;
constexpr int kFrequencyRetryCount = 5;
constexpr int kPcmFrameSamples = 1024;
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

double envDouble(const char *name, double fallback)
{
    const QByteArray value = qgetenv(name);
    if (value.isEmpty()) {
        return fallback;
    }

    bool ok = false;
    const double parsed = QString::fromLatin1(value).toDouble(&ok);
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

int configuredDirectSampling()
{
    return std::max(0, std::min(2, envInt("RTLSDR_AM_DIRECT_SAMPLING", 0)));
}

quint64 defaultFrequency()
{
    return configuredDirectSampling() != 0
        ? kDefaultDirectSamplingFrequency
        : kDefaultTunerFrequency;
}

quint64 defaultMinFrequency()
{
    return configuredDirectSampling() != 0
        ? kDefaultDirectSamplingMinFrequency
        : kDefaultTunerMinFrequency;
}

quint64 defaultMaxFrequency()
{
    return configuredDirectSampling() != 0
        ? kDefaultDirectSamplingMaxFrequency
        : kDefaultTunerMaxFrequency;
}

uint32_t sanitizedAudioRate()
{
    const quint64 configured = envUInt64("RTLSDR_AM_AUDIO_RATE", kDefaultAudioRate);
    const quint64 clamped = std::max<quint64>(8000ULL, std::min<quint64>(96000ULL, configured));
    return static_cast<uint32_t>(clamped);
}

uint32_t sanitizedSampleRate(uint32_t audioRate)
{
    const quint64 configured = envUInt64("RTLSDR_AM_SAMPLE_RATE", kDefaultSampleRate);
    const int decimation = std::max(2,
                                    std::min(64,
                                             static_cast<int>(std::llround(
                                                 static_cast<double>(configured)
                                                 / static_cast<double>(audioRate)))));
    return static_cast<uint32_t>(audioRate * static_cast<uint32_t>(decimation));
}

struct TunerProfile {
    QString name;
    quint64 minFrequencyHz = kDefaultTunerMinFrequency;
    quint64 maxFrequencyHz = kDefaultTunerMaxFrequency;
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
        return {QStringLiteral("未知 tuner"), kDefaultTunerMinFrequency, kDefaultTunerMaxFrequency};
    }
}

TunerProfile withConfiguredRange(TunerProfile profile, int directSampling)
{
    if (directSampling != 0) {
        profile.name += QStringLiteral(" / Direct sampling");
        profile.minFrequencyHz = kDefaultDirectSamplingMinFrequency;
        profile.maxFrequencyHz = kDefaultDirectSamplingMaxFrequency;
    }

    profile.minFrequencyHz = envUInt64("RTLSDR_AM_MIN_FREQ", profile.minFrequencyHz);
    profile.maxFrequencyHz = envUInt64("RTLSDR_AM_MAX_FREQ", profile.maxFrequencyHz);
    if (profile.minFrequencyHz > profile.maxFrequencyHz) {
        std::swap(profile.minFrequencyHz, profile.maxFrequencyHz);
    }
    return profile;
}

QString mhzLabel(double hz)
{
    return QStringLiteral("%1 MHz").arg(hz / 1000000.0, 0, 'f', 3);
}

QString rtlError(const QString &operation, int code)
{
    return QStringLiteral("%1 失败，错误码 %2").arg(operation).arg(code);
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

struct AmModeProfile {
    uint32_t sampleRate = 240000U;
    uint32_t audioRate = 48000U;
    uint32_t tunerBandwidth = 12000U;
    double audioCutoffHz = 5000.0;
};

AmModeProfile makeModeProfile(uint32_t sampleRate, uint32_t audioRate)
{
    AmModeProfile profile;
    profile.sampleRate = sampleRate;
    profile.audioRate = audioRate;
    profile.tunerBandwidth = static_cast<uint32_t>(
        std::max(5000, envInt("RTLSDR_AM_BANDWIDTH", 12000)));
    profile.audioCutoffHz = std::max(1200.0, std::min(12000.0, envDouble("RTLSDR_AM_AUDIO_CUTOFF", 5000.0)));
    profile.audioCutoffHz = std::min(profile.audioCutoffHz,
                                     static_cast<double>(profile.audioRate) * 0.45);
    return profile;
}

std::vector<double> lowPassTaps(int tapCount, double cutoffHz, double sampleRate)
{
    tapCount = std::max(31, tapCount | 1);
    const int center = tapCount / 2;
    const double normalizedCutoff = std::max(0.001, std::min(0.49, cutoffHz / sampleRate));

    std::vector<double> taps(static_cast<size_t>(tapCount));
    double sum = 0.0;
    for (int i = 0; i < tapCount; ++i) {
        const int n = i - center;
        const double sinc = n == 0
            ? 2.0 * normalizedCutoff
            : std::sin(2.0 * kPi * normalizedCutoff * n) / (kPi * n);
        const double window = 0.42
            - 0.5 * std::cos(2.0 * kPi * i / static_cast<double>(tapCount - 1))
            + 0.08 * std::cos(4.0 * kPi * i / static_cast<double>(tapCount - 1));
        taps[static_cast<size_t>(i)] = sinc * window;
        sum += taps[static_cast<size_t>(i)];
    }

    if (std::abs(sum) > 1.0e-12) {
        for (double &tap : taps) {
            tap /= sum;
        }
    }
    return taps;
}

QString audioCommand(uint32_t audioRate)
{
    QString command = QString::fromLocal8Bit(qgetenv("RTLSDR_AM_AUDIO_CMD")).trimmed();
    if (command.isEmpty()) {
        command = QStringLiteral("aplay -q -t raw -f S16_LE -r %1 -c 1").arg(audioRate);
    } else {
        command.replace(QStringLiteral("{rate}"), QString::number(audioRate));
        command.replace(QStringLiteral("%r"), QString::number(audioRate));
        command.replace(QStringLiteral("{channels}"), QStringLiteral("1"));
        command.replace(QStringLiteral("%c"), QStringLiteral("1"));
    }
    return command;
}

bool startAudioProcess(QProcess *audio, uint32_t audioRate, QString *error)
{
    const QString command = audioCommand(audioRate);
#if defined(Q_OS_WIN)
    audio->start(QStringLiteral("cmd.exe"), QStringList() << QStringLiteral("/C") << command);
#else
    audio->start(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-lc") << command);
#endif
    if (!audio->waitForStarted(3000)) {
        if (error != nullptr) {
            *error = QStringLiteral("启动音频输出失败：%1").arg(command);
        }
        return false;
    }
    return true;
}

bool writePcmFrame(QProcess *audio, const std::vector<qint16> &frame, QString *error)
{
    if (frame.empty()) {
        return true;
    }

    if (audio->state() != QProcess::Running) {
        if (error != nullptr) {
            *error = QStringLiteral("音频输出进程已退出");
        }
        return false;
    }

    const char *data = reinterpret_cast<const char *>(frame.data());
    const qint64 byteCount = static_cast<qint64>(frame.size() * sizeof(qint16));
    if (audio->write(data, byteCount) < 0) {
        if (error != nullptr) {
            *error = QStringLiteral("写入音频输出失败");
        }
        return false;
    }

    if (!audio->waitForBytesWritten(80) && audio->state() != QProcess::Running) {
        if (error != nullptr) {
            *error = QStringLiteral("音频输出进程中断");
        }
        return false;
    }

    return true;
}

class AmDemodulator
{
public:
    explicit AmDemodulator(const AmModeProfile &profile)
        : m_decimation(std::max(1U, profile.sampleRate / profile.audioRate))
        , m_taps(lowPassTaps(101,
                             profile.audioCutoffHz,
                             static_cast<double>(profile.sampleRate)))
        , m_ring(m_taps.size(), 0.0)
    {
    }

    void reset()
    {
        m_decimationPhase = 0;
        std::fill(m_ring.begin(), m_ring.end(), 0.0);
        m_ringIndex = 0;
        m_previousAudioInput = 0.0;
        m_dcOutput = 0.0;
        m_agcLevel = 0.02;
    }

    bool processSample(unsigned char rawI, unsigned char rawQ, double volume, qint16 *pcm)
    {
        const double inPhase = (static_cast<double>(rawI) - 127.5) / 128.0;
        const double quadrature = (static_cast<double>(rawQ) - 127.5) / 128.0;
        const double envelope = std::sqrt(inPhase * inPhase + quadrature * quadrature);

        m_ring[static_cast<size_t>(m_ringIndex)] = envelope;
        m_ringIndex = (m_ringIndex + 1) % static_cast<int>(m_ring.size());
        ++m_decimationPhase;
        if (m_decimationPhase < static_cast<int>(m_decimation)) {
            return false;
        }
        m_decimationPhase = 0;

        double filtered = 0.0;
        int index = m_ringIndex - 1;
        if (index < 0) {
            index = static_cast<int>(m_ring.size()) - 1;
        }
        for (size_t tapIndex = 0; tapIndex < m_taps.size(); ++tapIndex) {
            filtered += m_taps[tapIndex] * m_ring[static_cast<size_t>(index)];
            --index;
            if (index < 0) {
                index = static_cast<int>(m_ring.size()) - 1;
            }
        }

        const double dcBlocked = filtered - m_previousAudioInput + 0.995 * m_dcOutput;
        m_previousAudioInput = filtered;
        m_dcOutput = dcBlocked;

        const double absolute = std::abs(dcBlocked);
        const double agcSpeed = absolute > m_agcLevel ? 0.010 : 0.001;
        m_agcLevel += agcSpeed * (absolute - m_agcLevel);

        const double gain = 0.18 / std::max(0.004, m_agcLevel);
        double audio = dcBlocked * gain * volume;
        audio = std::max(-1.0, std::min(1.0, audio));
        *pcm = static_cast<qint16>(std::lrint(audio * 32767.0));
        return true;
    }

private:
    uint32_t m_decimation = 1;
    std::vector<double> m_taps;
    std::vector<double> m_ring;
    int m_ringIndex = 0;
    int m_decimationPhase = 0;
    double m_previousAudioInput = 0.0;
    double m_dcOutput = 0.0;
    double m_agcLevel = 0.02;
};
}

class RtlAmWorker : public QThread
{
    Q_OBJECT

public:
    RtlAmWorker(uint32_t deviceIndex,
                uint32_t centerFrequency,
                int ppm,
                int gain,
                bool manualGain,
                int directSampling,
                const AmModeProfile &modeProfile,
                double volume,
                QObject *parent = nullptr)
        : QThread(parent)
        , m_deviceIndex(deviceIndex)
        , m_ppm(ppm)
        , m_gain(gain)
        , m_manualGain(manualGain)
        , m_directSampling(directSampling)
        , m_modeProfile(modeProfile)
        , m_requestedFrequency(centerFrequency)
        , m_volumePermille(static_cast<int>(std::llround(volume * 1000.0)))
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

    void requestVolume(double volume)
    {
        const int permille = std::max(0, std::min(1500, static_cast<int>(std::llround(volume * 1000.0))));
        m_volumePermille.store(permille);
    }

signals:
    void statusReady(const QString &status);
    void deviceCountReady(int count);
    void tunerInfoReady(const QString &name, double minFrequencyHz, double maxFrequencyHz);
    void centerFrequencyReady(double frequencyHz);
    void audioLevelReady(double level);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        emit statusReady(QStringLiteral("正在初始化 RTL-SDR AM 接收机"));

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

        TunerProfile tunerProfile = withConfiguredRange(profileFor(rtlsdr_get_tuner_type(device)), m_directSampling);
        emit tunerInfoReady(tunerProfile.name,
                            static_cast<double>(tunerProfile.minFrequencyHz),
                            static_cast<double>(tunerProfile.maxFrequencyHz));

        result = rtlsdr_set_sample_rate(device, m_modeProfile.sampleRate);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("设置采样率"), result));
            closeDevice();
            return;
        }

        rtlsdr_set_freq_correction(device, m_ppm);
        rtlsdr_set_agc_mode(device, 1);
        rtlsdr_set_direct_sampling(device, m_directSampling);
        rtlsdr_set_offset_tuning(device, 0);
        rtlsdr_set_tuner_bandwidth(device, m_modeProfile.tunerBandwidth);
        if (m_manualGain) {
            rtlsdr_set_tuner_gain_mode(device, 1);
            rtlsdr_set_tuner_gain(device, m_gain);
        } else {
            rtlsdr_set_tuner_gain_mode(device, 0);
        }

        uint32_t currentFrequency = boundedRequestedFrequency(tunerProfile);
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

        QProcess audio;
        QString audioError;
        if (!startAudioProcess(&audio, m_modeProfile.audioRate, &audioError)) {
            emit statusReady(audioError);
            closeDevice();
            return;
        }

        emit statusReady(QStringLiteral("AM 接收中：%1 / %2 SPS / 音频 %3 Hz / %4")
                             .arg(mhzLabel(currentFrequency))
                             .arg(m_modeProfile.sampleRate)
                             .arg(m_modeProfile.audioRate)
                             .arg(tunerProfile.name));

        AmDemodulator demodulator(m_modeProfile);
        std::vector<unsigned char> raw(kReadBufferBytes);
        std::vector<qint16> pcmFrame;
        pcmFrame.reserve(kPcmFrameSamples);
        QElapsedTimer levelTimer;
        levelTimer.start();
        double levelEnergy = 0.0;
        int levelSamples = 0;

        while (!m_stopRequested.load()) {
            const uint32_t requested = boundedRequestedFrequency(tunerProfile);
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
                demodulator.reset();
                pcmFrame.clear();
                emit statusReady(QStringLiteral("AM 已调谐到 %1").arg(mhzLabel(currentFrequency)));
            }

            int bytesRead = 0;
            result = rtlsdr_read_sync(device, raw.data(), static_cast<int>(raw.size()), &bytesRead);
            if (result < 0) {
                emit statusReady(rtlError(QStringLiteral("读取 RTL-SDR 数据"), result));
                break;
            }
            if (bytesRead <= 1) {
                continue;
            }

            const double volume = static_cast<double>(m_volumePermille.load()) / 1000.0;
            const int availableBytes = bytesRead & ~1;
            for (int offset = 0; offset + 1 < availableBytes; offset += 2) {
                qint16 pcm = 0;
                if (!demodulator.processSample(raw[static_cast<size_t>(offset)],
                                               raw[static_cast<size_t>(offset + 1)],
                                               volume,
                                               &pcm)) {
                    continue;
                }

                pcmFrame.push_back(pcm);
                const double normalized = static_cast<double>(pcm) / 32768.0;
                levelEnergy += normalized * normalized;
                ++levelSamples;

                if (pcmFrame.size() >= kPcmFrameSamples) {
                    if (!writePcmFrame(&audio, pcmFrame, &audioError)) {
                        emit statusReady(audioError);
                        m_stopRequested.store(true);
                        break;
                    }
                    pcmFrame.clear();
                }

                if (levelTimer.elapsed() >= 100 && levelSamples > 0) {
                    const double rms = std::sqrt(levelEnergy / static_cast<double>(levelSamples));
                    emit audioLevelReady(std::max(0.0, std::min(1.0, rms * 3.0)));
                    levelEnergy = 0.0;
                    levelSamples = 0;
                    levelTimer.restart();
                }
            }
        }

        if (!pcmFrame.empty()) {
            writePcmFrame(&audio, pcmFrame, nullptr);
        }
        audio.closeWriteChannel();
        audio.waitForFinished(600);
        if (audio.state() != QProcess::NotRunning) {
            audio.kill();
            audio.waitForFinished(300);
        }
        closeDevice();
        emit audioLevelReady(0.0);
        emit statusReady(QStringLiteral("AM 已停止"));
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
    int m_ppm = 0;
    int m_gain = 0;
    bool m_manualGain = false;
    int m_directSampling = 0;
    AmModeProfile m_modeProfile;
    std::atomic_bool m_stopRequested {false};
    std::atomic<uint32_t> m_requestedFrequency;
    std::atomic<int> m_volumePermille;
};

AmReceiver::AmReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("AM 未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_tunerName(QStringLiteral("未检测"))
    , m_centerFrequencyHz(static_cast<double>(envUInt64("RTLSDR_AM_FREQ", defaultFrequency())))
    , m_sampleRateHz(static_cast<double>(sanitizedSampleRate(sanitizedAudioRate())))
    , m_audioRateHz(static_cast<double>(sanitizedAudioRate()))
    , m_minFrequencyHz(static_cast<double>(envUInt64("RTLSDR_AM_MIN_FREQ", defaultMinFrequency())))
    , m_maxFrequencyHz(static_cast<double>(envUInt64("RTLSDR_AM_MAX_FREQ", defaultMaxFrequency())))
    , m_volume(std::max(0.0, std::min(1.5, envDouble("RTLSDR_AM_VOLUME", 0.80))))
{
    if (m_minFrequencyHz > m_maxFrequencyHz) {
        std::swap(m_minFrequencyHz, m_maxFrequencyHz);
    }
    m_centerFrequencyHz = clampedFrequency(m_centerFrequencyHz);
}

AmReceiver::~AmReceiver()
{
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(2000);
    }
}

bool AmReceiver::running() const
{
    return m_running;
}

QString AmReceiver::status() const
{
    return m_status;
}

int AmReceiver::deviceCount() const
{
    return m_deviceCount;
}

QString AmReceiver::tunerName() const
{
    return m_tunerName;
}

double AmReceiver::centerFrequencyHz() const
{
    return m_centerFrequencyHz;
}

double AmReceiver::sampleRateHz() const
{
    return m_sampleRateHz;
}

double AmReceiver::audioRateHz() const
{
    return m_audioRateHz;
}

double AmReceiver::minFrequencyHz() const
{
    return m_minFrequencyHz;
}

double AmReceiver::maxFrequencyHz() const
{
    return m_maxFrequencyHz;
}

double AmReceiver::audioLevel() const
{
    return m_audioLevel;
}

double AmReceiver::volume() const
{
    return m_volume;
}

void AmReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    setAudioLevel(0.0);
    setStatus(QStringLiteral("正在启动 AM 接收机"));
    const uint32_t deviceIndex = static_cast<uint32_t>(std::max(0, envInt("RTLSDR_AM_DEVICE", 0)));
    const int ppm = envInt("RTLSDR_AM_PPM", 0);
    const QByteArray gainEnv = qgetenv("RTLSDR_AM_GAIN");
    bool gainOk = false;
    const int gain = QString::fromLatin1(gainEnv).toInt(&gainOk);
    const int directSampling = configuredDirectSampling();
    const AmModeProfile profile = makeModeProfile(clampToUInt32(m_sampleRateHz), clampToUInt32(m_audioRateHz));

    m_worker = new RtlAmWorker(deviceIndex,
                               clampToUInt32(m_centerFrequencyHz),
                               ppm,
                               gain,
                               gainOk,
                               directSampling,
                               profile,
                               m_volume);
    connect(m_worker, &RtlAmWorker::statusReady, this, &AmReceiver::setStatus);
    connect(m_worker, &RtlAmWorker::deviceCountReady, this, &AmReceiver::setDeviceCount);
    connect(m_worker, &RtlAmWorker::tunerInfoReady, this, &AmReceiver::setTunerInfo);
    connect(m_worker, &RtlAmWorker::centerFrequencyReady, this, &AmReceiver::setCenterFrequencyFromWorker);
    connect(m_worker, &RtlAmWorker::audioLevelReady, this, &AmReceiver::setAudioLevel);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        setRunning(false);
    });

    setRunning(true);
    m_worker->start();
}

void AmReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在停止 AM 接收机"));
    m_worker->requestStop();
}

void AmReceiver::tuneMHz(double mhz)
{
    setCenterFrequencyHz(mhz * 1000000.0);
}

void AmReceiver::stepMHz(double deltaMhz)
{
    setCenterFrequencyHz(m_centerFrequencyHz + deltaMhz * 1000000.0);
}

void AmReceiver::setVolume(double volume)
{
    if (!std::isfinite(volume)) {
        return;
    }

    volume = std::max(0.0, std::min(1.5, volume));
    if (qFuzzyCompare(m_volume, volume)) {
        return;
    }

    m_volume = volume;
    emit volumeChanged();

    if (m_worker != nullptr) {
        m_worker->requestVolume(m_volume);
    }
}

void AmReceiver::setCenterFrequencyHz(double frequencyHz)
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

void AmReceiver::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void AmReceiver::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

void AmReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) {
        return;
    }

    m_deviceCount = count;
    emit deviceCountChanged();
}

void AmReceiver::setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz)
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

void AmReceiver::setCenterFrequencyFromWorker(double frequencyHz)
{
    frequencyHz = clampedFrequency(frequencyHz);
    if (qFuzzyCompare(m_centerFrequencyHz, frequencyHz)) {
        return;
    }

    m_centerFrequencyHz = frequencyHz;
    emit centerFrequencyHzChanged();
}

void AmReceiver::setAudioLevel(double level)
{
    level = std::max(0.0, std::min(1.0, level));
    if (std::abs(m_audioLevel - level) < 0.01) {
        return;
    }

    m_audioLevel = level;
    emit audioLevelChanged();
}

double AmReceiver::clampedFrequency(double frequencyHz) const
{
    return std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, frequencyHz));
}

bool AmReceiver::frequencyInRange(double frequencyHz) const
{
    return frequencyHz >= m_minFrequencyHz && frequencyHz <= m_maxFrequencyHz;
}

#include "AmReceiver.moc"
