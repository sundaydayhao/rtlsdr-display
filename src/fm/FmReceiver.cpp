#include "FmReceiver.h"

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
constexpr quint64 kDefaultFrequency = 100700000ULL;
constexpr quint64 kDefaultAudioRate = 48000ULL;
constexpr quint64 kDefaultMinFrequency = 24000000ULL;
constexpr quint64 kDefaultMaxFrequency = 1766000000ULL;
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

uint32_t sanitizedAudioRate()
{
    const quint64 configured = envUInt64("RTLSDR_FM_AUDIO_RATE", kDefaultAudioRate);
    const quint64 clamped = std::max<quint64>(8000ULL, std::min<quint64>(96000ULL, configured));
    return static_cast<uint32_t>(clamped);
}

uint32_t sampleRateForMode(const QString &mode, uint32_t audioRate)
{
    const quint64 fallback = mode == QStringLiteral("nfm") ? 240000ULL : 1200000ULL;
    const char *envName = mode == QStringLiteral("nfm")
        ? "RTLSDR_FM_NFM_SAMPLE_RATE"
        : "RTLSDR_FM_WFM_SAMPLE_RATE";
    const quint64 configured = envUInt64(envName, fallback);
    const int minDecimation = mode == QStringLiteral("nfm") ? 4 : 10;
    const int maxDecimation = mode == QStringLiteral("nfm") ? 16 : 64;
    const int decimation = std::max(minDecimation,
                                    std::min(maxDecimation,
                                             static_cast<int>(std::llround(
                                                 static_cast<double>(configured)
                                                 / static_cast<double>(audioRate)))));
    return static_cast<uint32_t>(audioRate * static_cast<uint32_t>(decimation));
}

QString normalizedFmMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("nfm") || normalized == QStringLiteral("narrow")) {
        return QStringLiteral("nfm");
    }
    return QStringLiteral("wfm");
}

QString modeLabel(const QString &mode)
{
    return mode == QStringLiteral("nfm")
        ? QStringLiteral("NFM 窄带语音")
        : QStringLiteral("WFM 广播");
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
    profile.minFrequencyHz = envUInt64("RTLSDR_FM_MIN_FREQ", profile.minFrequencyHz);
    profile.maxFrequencyHz = envUInt64("RTLSDR_FM_MAX_FREQ", profile.maxFrequencyHz);
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

struct FmModeProfile {
    QString id;
    QString label;
    uint32_t sampleRate = 1200000U;
    uint32_t audioRate = 48000U;
    uint32_t tunerBandwidth = 200000U;
    double audioCutoffHz = 16000.0;
    double deviationHz = 75000.0;
    double deemphasisUs = 50.0;
};

FmModeProfile makeModeProfile(const QString &mode, uint32_t audioRate)
{
    FmModeProfile profile;
    profile.id = normalizedFmMode(mode);
    profile.label = modeLabel(profile.id);
    profile.audioRate = audioRate;
    profile.sampleRate = sampleRateForMode(profile.id, audioRate);

    if (profile.id == QStringLiteral("nfm")) {
        profile.tunerBandwidth = static_cast<uint32_t>(
            std::max(5000, envInt("RTLSDR_FM_NFM_BANDWIDTH", 12500)));
        profile.audioCutoffHz = std::max(1200.0, std::min(12000.0, envDouble("RTLSDR_FM_NFM_AUDIO_CUTOFF", 5500.0)));
        profile.deviationHz = std::max(1000.0, std::min(15000.0, envDouble("RTLSDR_FM_NFM_DEVIATION", 5000.0)));
        profile.deemphasisUs = std::max(0.0, std::min(1000.0, envDouble("RTLSDR_FM_NFM_DEEMPHASIS_US", 0.0)));
    } else {
        profile.tunerBandwidth = static_cast<uint32_t>(
            std::max(50000, envInt("RTLSDR_FM_WFM_BANDWIDTH", 200000)));
        profile.audioCutoffHz = std::max(8000.0, std::min(22000.0, envDouble("RTLSDR_FM_WFM_AUDIO_CUTOFF", 16000.0)));
        profile.deviationHz = std::max(25000.0, std::min(120000.0, envDouble("RTLSDR_FM_WFM_DEVIATION", 75000.0)));
        profile.deemphasisUs = std::max(0.0, std::min(1000.0, envDouble("RTLSDR_FM_WFM_DEEMPHASIS_US", 50.0)));
    }

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
    QString command = QString::fromLocal8Bit(qgetenv("RTLSDR_FM_AUDIO_CMD")).trimmed();
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

class FmDemodulator
{
public:
    explicit FmDemodulator(const FmModeProfile &profile)
        : m_profile(profile)
        , m_decimation(std::max(1U, profile.sampleRate / profile.audioRate))
        , m_fullScaleRadians(std::max(1.0e-6,
                                      2.0 * kPi * profile.deviationHz
                                      / static_cast<double>(profile.sampleRate)))
        , m_taps(lowPassTaps(profile.id == QStringLiteral("nfm") ? 81 : 101,
                             profile.audioCutoffHz,
                             static_cast<double>(profile.sampleRate)))
        , m_ring(m_taps.size(), 0.0)
    {
        if (profile.deemphasisUs > 0.0) {
            const double dt = 1.0 / static_cast<double>(profile.audioRate);
            const double tau = profile.deemphasisUs / 1000000.0;
            m_deemphasisAlpha = dt / (tau + dt);
        }
    }

    void reset()
    {
        m_hasPreviousIq = false;
        m_previousI = 0.0;
        m_previousQ = 0.0;
        m_decimationPhase = 0;
        std::fill(m_ring.begin(), m_ring.end(), 0.0);
        m_ringIndex = 0;
        m_previousAudioInput = 0.0;
        m_dcOutput = 0.0;
        m_deemphasisOutput = 0.0;
    }

    bool processSample(unsigned char rawI, unsigned char rawQ, double volume, qint16 *pcm)
    {
        const double inPhase = (static_cast<double>(rawI) - 127.5) / 128.0;
        const double quadrature = (static_cast<double>(rawQ) - 127.5) / 128.0;

        if (!m_hasPreviousIq) {
            m_previousI = inPhase;
            m_previousQ = quadrature;
            m_hasPreviousIq = true;
            return false;
        }

        const double real = inPhase * m_previousI + quadrature * m_previousQ;
        const double imag = quadrature * m_previousI - inPhase * m_previousQ;
        const double demodulated = std::atan2(imag, real);
        m_previousI = inPhase;
        m_previousQ = quadrature;

        m_ring[static_cast<size_t>(m_ringIndex)] = demodulated;
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

        double audio = dcBlocked;
        if (m_deemphasisAlpha > 0.0) {
            m_deemphasisOutput += m_deemphasisAlpha * (audio - m_deemphasisOutput);
            audio = m_deemphasisOutput;
        }

        audio = (audio / m_fullScaleRadians) * volume;
        audio = std::max(-1.0, std::min(1.0, audio));
        *pcm = static_cast<qint16>(std::lrint(audio * 32767.0));
        return true;
    }

private:
    FmModeProfile m_profile;
    uint32_t m_decimation = 1;
    double m_fullScaleRadians = 1.0;
    std::vector<double> m_taps;
    std::vector<double> m_ring;
    int m_ringIndex = 0;
    int m_decimationPhase = 0;
    bool m_hasPreviousIq = false;
    double m_previousI = 0.0;
    double m_previousQ = 0.0;
    double m_previousAudioInput = 0.0;
    double m_dcOutput = 0.0;
    double m_deemphasisAlpha = 0.0;
    double m_deemphasisOutput = 0.0;
};
}

class RtlFmWorker : public QThread
{
    Q_OBJECT

public:
    RtlFmWorker(uint32_t deviceIndex,
                uint32_t centerFrequency,
                int ppm,
                int gain,
                bool manualGain,
                int directSampling,
                const FmModeProfile &modeProfile,
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
        emit statusReady(QStringLiteral("正在初始化 RTL-SDR FM 接收机"));

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

        TunerProfile tunerProfile = withConfiguredRange(profileFor(rtlsdr_get_tuner_type(device)));
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

        emit statusReady(QStringLiteral("FM 接收中：%1 / %2 / %3 SPS / 音频 %4 Hz / %5")
                             .arg(mhzLabel(currentFrequency),
                                  m_modeProfile.label)
                             .arg(m_modeProfile.sampleRate)
                             .arg(m_modeProfile.audioRate)
                             .arg(tunerProfile.name));

        FmDemodulator demodulator(m_modeProfile);
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
                emit statusReady(QStringLiteral("FM 已调谐到 %1").arg(mhzLabel(currentFrequency)));
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
        emit statusReady(QStringLiteral("FM 已停止"));
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
    FmModeProfile m_modeProfile;
    std::atomic_bool m_stopRequested {false};
    std::atomic<uint32_t> m_requestedFrequency;
    std::atomic<int> m_volumePermille;
};

FmReceiver::FmReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("FM 未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_tunerName(QStringLiteral("未检测"))
    , m_centerFrequencyHz(static_cast<double>(envUInt64("RTLSDR_FM_FREQ", kDefaultFrequency)))
    , m_audioRateHz(static_cast<double>(sanitizedAudioRate()))
    , m_minFrequencyHz(static_cast<double>(envUInt64("RTLSDR_FM_MIN_FREQ", kDefaultMinFrequency)))
    , m_maxFrequencyHz(static_cast<double>(envUInt64("RTLSDR_FM_MAX_FREQ", kDefaultMaxFrequency)))
    , m_mode(normalizedFmMode(QString::fromLocal8Bit(qgetenv("RTLSDR_FM_MODE"))))
    , m_volume(std::max(0.0, std::min(1.5, envDouble("RTLSDR_FM_VOLUME", 0.80))))
{
    if (m_minFrequencyHz > m_maxFrequencyHz) {
        std::swap(m_minFrequencyHz, m_maxFrequencyHz);
    }
    m_centerFrequencyHz = clampedFrequency(m_centerFrequencyHz);
}

FmReceiver::~FmReceiver()
{
    m_restartAfterStop = false;
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(2000);
    }
}

bool FmReceiver::running() const
{
    return m_running;
}

QString FmReceiver::status() const
{
    return m_status;
}

int FmReceiver::deviceCount() const
{
    return m_deviceCount;
}

QString FmReceiver::tunerName() const
{
    return m_tunerName;
}

double FmReceiver::centerFrequencyHz() const
{
    return m_centerFrequencyHz;
}

double FmReceiver::sampleRateHz() const
{
    return sampleRateForMode(m_mode);
}

double FmReceiver::audioRateHz() const
{
    return m_audioRateHz;
}

double FmReceiver::minFrequencyHz() const
{
    return m_minFrequencyHz;
}

double FmReceiver::maxFrequencyHz() const
{
    return m_maxFrequencyHz;
}

QString FmReceiver::mode() const
{
    return m_mode;
}

QString FmReceiver::modeLabel() const
{
    return ::modeLabel(m_mode);
}

double FmReceiver::audioLevel() const
{
    return m_audioLevel;
}

double FmReceiver::volume() const
{
    return m_volume;
}

void FmReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    setAudioLevel(0.0);
    setStatus(QStringLiteral("正在启动 FM 接收机"));
    const uint32_t deviceIndex = static_cast<uint32_t>(std::max(0, envInt("RTLSDR_FM_DEVICE", 0)));
    const int ppm = envInt("RTLSDR_FM_PPM", 0);
    const QByteArray gainEnv = qgetenv("RTLSDR_FM_GAIN");
    bool gainOk = false;
    const int gain = QString::fromLatin1(gainEnv).toInt(&gainOk);
    const int directSampling = std::max(0, std::min(2, envInt("RTLSDR_FM_DIRECT_SAMPLING", 0)));
    const FmModeProfile profile = makeModeProfile(m_mode, clampToUInt32(m_audioRateHz));

    m_worker = new RtlFmWorker(deviceIndex,
                               clampToUInt32(m_centerFrequencyHz),
                               ppm,
                               gain,
                               gainOk,
                               directSampling,
                               profile,
                               m_volume);
    connect(m_worker, &RtlFmWorker::statusReady, this, &FmReceiver::setStatus);
    connect(m_worker, &RtlFmWorker::deviceCountReady, this, &FmReceiver::setDeviceCount);
    connect(m_worker, &RtlFmWorker::tunerInfoReady, this, &FmReceiver::setTunerInfo);
    connect(m_worker, &RtlFmWorker::centerFrequencyReady, this, &FmReceiver::setCenterFrequencyFromWorker);
    connect(m_worker, &RtlFmWorker::audioLevelReady, this, &FmReceiver::setAudioLevel);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        setRunning(false);
        if (m_restartAfterStop) {
            m_restartAfterStop = false;
            start();
        }
    });

    setRunning(true);
    m_worker->start();
}

void FmReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在停止 FM 接收机"));
    m_worker->requestStop();
}

void FmReceiver::tuneMHz(double mhz)
{
    setCenterFrequencyHz(mhz * 1000000.0);
}

void FmReceiver::stepMHz(double deltaMhz)
{
    setCenterFrequencyHz(m_centerFrequencyHz + deltaMhz * 1000000.0);
}

void FmReceiver::setMode(const QString &mode)
{
    const QString normalized = normalizedMode(mode);
    if (m_mode == normalized) {
        return;
    }

    m_mode = normalized;
    emit modeChanged();
    emit sampleRateHzChanged();

    if (m_worker != nullptr) {
        m_restartAfterStop = true;
        stop();
    }
}

void FmReceiver::setVolume(double volume)
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

void FmReceiver::setCenterFrequencyHz(double frequencyHz)
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

void FmReceiver::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void FmReceiver::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

void FmReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) {
        return;
    }

    m_deviceCount = count;
    emit deviceCountChanged();
}

void FmReceiver::setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz)
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

void FmReceiver::setCenterFrequencyFromWorker(double frequencyHz)
{
    frequencyHz = clampedFrequency(frequencyHz);
    if (qFuzzyCompare(m_centerFrequencyHz, frequencyHz)) {
        return;
    }

    m_centerFrequencyHz = frequencyHz;
    emit centerFrequencyHzChanged();
}

void FmReceiver::setAudioLevel(double level)
{
    level = std::max(0.0, std::min(1.0, level));
    if (std::abs(m_audioLevel - level) < 0.01) {
        return;
    }

    m_audioLevel = level;
    emit audioLevelChanged();
}

double FmReceiver::clampedFrequency(double frequencyHz) const
{
    return std::max(m_minFrequencyHz, std::min(m_maxFrequencyHz, frequencyHz));
}

bool FmReceiver::frequencyInRange(double frequencyHz) const
{
    return frequencyHz >= m_minFrequencyHz && frequencyHz <= m_maxFrequencyHz;
}

double FmReceiver::sampleRateForMode(const QString &mode) const
{
    return static_cast<double>(::sampleRateForMode(normalizedMode(mode), clampToUInt32(m_audioRateHz)));
}

QString FmReceiver::normalizedMode(const QString &mode) const
{
    return normalizedFmMode(mode);
}

#include "FmReceiver.moc"
