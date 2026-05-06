#include "AdsbReceiver.h"

#include "AdsbDecoder.h"

#include <QDateTime>
#include <QHash>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include <rtl-sdr.h>

namespace {
constexpr uint32_t kDefaultFrequency = 1090000000U;
constexpr uint32_t kDefaultSampleRate = 2000000U;
constexpr int kReadBufferBytes = 256 * 1024;
constexpr int kTailSamples = 512;
constexpr int kCprMaxAgeMs = 10000;
constexpr int kFrequencyRetryCount = 5;

int envInt(const char *name, int fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok ? value : fallback;
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

uint32_t magnitude(unsigned char i, unsigned char q)
{
    const int centeredI = static_cast<int>(i) - 127;
    const int centeredQ = static_cast<int>(q) - 127;
    return static_cast<uint32_t>(centeredI * centeredI + centeredQ * centeredQ);
}

QString rtlError(const QString &operation, int code)
{
    return QStringLiteral("%1 失败，错误码 %2").arg(operation).arg(code);
}

QString tunerName(enum rtlsdr_tuner tuner)
{
    switch (tuner) {
    case RTLSDR_TUNER_E4000:
        return QStringLiteral("Elonics E4000");
    case RTLSDR_TUNER_FC0012:
        return QStringLiteral("Fitipower FC0012");
    case RTLSDR_TUNER_FC0013:
        return QStringLiteral("Fitipower FC0013");
    case RTLSDR_TUNER_FC2580:
        return QStringLiteral("FCI FC2580");
    case RTLSDR_TUNER_R820T:
        return QStringLiteral("Rafael R820T/R820T2");
    case RTLSDR_TUNER_R828D:
        return QStringLiteral("Rafael R828D/R860");
    case RTLSDR_TUNER_UNKNOWN:
    default:
        return QStringLiteral("未知 tuner");
    }
}

bool tunerSupports1090MHz(enum rtlsdr_tuner tuner)
{
    return tuner != RTLSDR_TUNER_FC0012 && tuner != RTLSDR_TUNER_FC2580;
}

bool tuneWithRetry(rtlsdr_dev_t *device, uint32_t frequency, QString *error)
{
    for (int attempt = 1; attempt <= kFrequencyRetryCount; ++attempt) {
        const int result = rtlsdr_set_center_freq(device, frequency);
        if (result >= 0) {
            const uint32_t tuned = rtlsdr_get_center_freq(device);
            if (tuned == frequency) {
                return true;
            }
        }

        QThread::msleep(120);
    }

    if (error != nullptr) {
        *error = QStringLiteral("设置中心频率失败：%1 Hz 多次锁频未成功").arg(frequency);
    }
    return false;
}
}

class RtlAdsbWorker : public QThread
{
    Q_OBJECT

public:
    explicit RtlAdsbWorker(QObject *parent = nullptr)
        : QThread(parent)
    {
    }

    void requestStop()
    {
        m_stopRequested.store(true);
    }

signals:
    void statusReady(const QString &status);
    void deviceCountReady(int count);
    void countersReady(qulonglong totalCandidates, qulonglong validMessages);
    void aircraftReady(const QVariantMap &aircraft);

protected:
    void run() override
    {
        m_stopRequested.store(false);
        emit statusReady(QStringLiteral("正在初始化 RTL-SDR ADS-B 接收机"));

        const uint32_t deviceCount = rtlsdr_get_device_count();
        emit deviceCountReady(static_cast<int>(deviceCount));
        if (deviceCount == 0) {
            emit statusReady(QStringLiteral("未找到 RTL-SDR 设备"));
            return;
        }

        rtlsdr_dev_t *device = nullptr;
        const uint32_t deviceIndex = static_cast<uint32_t>(std::max(0, envInt("RTLSDR_ADSB_DEVICE", 0)));
        int result = rtlsdr_open(&device, std::min(deviceIndex, deviceCount - 1));
        if (result < 0 || device == nullptr) {
            emit statusReady(rtlError(QStringLiteral("打开 RTL-SDR"), result));
            return;
        }

        const auto closeDevice = [&]() {
            rtlsdr_close(device);
            device = nullptr;
        };

        const uint32_t sampleRate = static_cast<uint32_t>(std::max(900001, envInt("RTLSDR_ADSB_SAMPLE_RATE", kDefaultSampleRate)));
        const uint32_t frequency = static_cast<uint32_t>(std::max(1, envInt("RTLSDR_ADSB_FREQ", kDefaultFrequency)));
        const int ppm = envInt("RTLSDR_ADSB_PPM", 0);
        const enum rtlsdr_tuner tuner = rtlsdr_get_tuner_type(device);
        const QString tunerLabel = tunerName(tuner);

        if (frequency == kDefaultFrequency && !tunerSupports1090MHz(tuner)) {
            emit statusReady(QStringLiteral("%1 不支持 1090 MHz ADS-B，请更换 R820T/R820T2/R828D/R860 类 RTL-SDR").arg(tunerLabel));
            closeDevice();
            return;
        }

        result = rtlsdr_set_sample_rate(device, sampleRate);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("设置采样率"), result));
            closeDevice();
            return;
        }

        result = rtlsdr_set_center_freq(device, frequency);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("设置中心频率"), result));
            closeDevice();
            return;
        }

        rtlsdr_set_freq_correction(device, ppm);
        rtlsdr_set_agc_mode(device, 1);
        rtlsdr_set_direct_sampling(device, 0);
        rtlsdr_set_offset_tuning(device, 0);
        rtlsdr_set_tuner_bandwidth(device, 0);

        const QByteArray gainEnv = qgetenv("RTLSDR_ADSB_GAIN");
        if (gainEnv.isEmpty()) {
            rtlsdr_set_tuner_gain_mode(device, 0);
        } else {
            bool ok = false;
            const int gain = QString::fromLatin1(gainEnv).toInt(&ok);
            if (ok) {
                rtlsdr_set_tuner_gain_mode(device, 1);
                rtlsdr_set_tuner_gain(device, gain);
            }
        }

        QString tuneError;
        if (!tuneWithRetry(device, frequency, &tuneError)) {
            emit statusReady(tuneError + QStringLiteral("，请检查天线、USB 供电或重插 RTL-SDR"));
            closeDevice();
            return;
        }

        result = rtlsdr_reset_buffer(device);
        if (result < 0) {
            emit statusReady(rtlError(QStringLiteral("重置缓冲区"), result));
            closeDevice();
            return;
        }

        emit statusReady(QStringLiteral("ADS-B 接收中：1090 MHz / %1 SPS / %2").arg(sampleRate).arg(tunerLabel));

        std::vector<unsigned char> raw(kReadBufferBytes);
        std::vector<uint32_t> tail;
        tail.reserve(kTailSamples);

        AdsbDecoder decoder;
        quint64 totalCandidates = 0;
        quint64 validMessages = 0;

        while (!m_stopRequested.load()) {
            int bytesRead = 0;
            result = rtlsdr_read_sync(device, raw.data(), static_cast<int>(raw.size()), &bytesRead);
            if (result < 0) {
                emit statusReady(rtlError(QStringLiteral("读取 RTL-SDR 数据"), result));
                break;
            }
            if (bytesRead <= 0) {
                continue;
            }

            const int previousTailSize = static_cast<int>(tail.size());
            std::vector<uint32_t> magnitudes;
            magnitudes.reserve(previousTailSize + bytesRead / 2);
            magnitudes.insert(magnitudes.end(), tail.begin(), tail.end());

            for (int offset = 0; offset + 1 < bytesRead; offset += 2) {
                magnitudes.push_back(magnitude(raw[offset], raw[offset + 1]));
            }

            const int scanStart = std::max(0, previousTailSize - 240);
            const AdsbDecoder::BlockStats stats = decoder.processMagnitudes(
                magnitudes,
                scanStart,
                [this](const std::array<uint8_t, 14> &message) {
                    handleMessage(message);
                });

            totalCandidates += stats.candidates;
            validMessages += stats.validMessages;
            emit countersReady(totalCandidates, validMessages);

            const int keep = std::min<int>(kTailSamples, static_cast<int>(magnitudes.size()));
            tail.assign(magnitudes.end() - keep, magnitudes.end());
        }

        closeDevice();
        emit statusReady(QStringLiteral("ADS-B 已停止"));
    }

private:
    struct CprFrame {
        bool valid = false;
        int lat = 0;
        int lon = 0;
        qint64 timeMs = 0;
    };

    struct AircraftState {
        QString icao;
        QString callsign;
        bool hasAltitude = false;
        int altitude = 0;
        bool hasPosition = false;
        double latitude = 0.0;
        double longitude = 0.0;
        bool hasSpeed = false;
        double speed = 0.0;
        bool hasHeading = false;
        double heading = 0.0;
        int messages = 0;
        qint64 lastSeenMs = 0;
        CprFrame even;
        CprFrame odd;
    };

    void handleMessage(const std::array<uint8_t, 14> &message)
    {
        DecodedAdsbMessage decoded;
        if (!AdsbDecoder::decodeMessage(message, &decoded)) {
            return;
        }

        AircraftState &state = m_aircraft[decoded.icao];
        state.icao = decoded.icao;
        state.messages += 1;
        state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

        if (!decoded.callsign.isEmpty()) {
            state.callsign = decoded.callsign;
        }
        if (decoded.hasAltitude) {
            state.hasAltitude = true;
            state.altitude = decoded.altitudeFeet;
        }
        if (decoded.hasSpeed) {
            state.hasSpeed = true;
            state.speed = decoded.speedKnots;
        }
        if (decoded.hasHeading) {
            state.hasHeading = true;
            state.heading = decoded.headingDegrees;
        }
        if (decoded.hasCpr) {
            CprFrame &frame = decoded.cprOdd ? state.odd : state.even;
            frame.valid = true;
            frame.lat = decoded.cprLatitude;
            frame.lon = decoded.cprLongitude;
            frame.timeMs = state.lastSeenMs;

            if (state.even.valid && state.odd.valid
                && std::llabs(state.even.timeMs - state.odd.timeMs) <= kCprMaxAgeMs) {
                double latitude = 0.0;
                double longitude = 0.0;
                const bool newestOdd = state.odd.timeMs > state.even.timeMs;
                if (AdsbDecoder::decodeGlobalCpr(newestOdd,
                                                 state.even.lat,
                                                 state.even.lon,
                                                 state.odd.lat,
                                                 state.odd.lon,
                                                 &latitude,
                                                 &longitude)) {
                    state.hasPosition = true;
                    state.latitude = latitude;
                    state.longitude = longitude;
                }
            }
        }

        QVariantMap map;
        map.insert(QStringLiteral("icao"), state.icao);
        map.insert(QStringLiteral("callsign"), state.callsign);
        map.insert(QStringLiteral("hasAltitude"), state.hasAltitude);
        map.insert(QStringLiteral("altitude"), state.altitude);
        map.insert(QStringLiteral("hasPosition"), state.hasPosition);
        map.insert(QStringLiteral("latitude"), state.latitude);
        map.insert(QStringLiteral("longitude"), state.longitude);
        map.insert(QStringLiteral("hasSpeed"), state.hasSpeed);
        map.insert(QStringLiteral("speed"), state.speed);
        map.insert(QStringLiteral("hasHeading"), state.hasHeading);
        map.insert(QStringLiteral("heading"), state.heading);
        map.insert(QStringLiteral("messages"), state.messages);
        map.insert(QStringLiteral("lastSeenMs"), state.lastSeenMs);
        emit aircraftReady(map);
    }

    std::atomic_bool m_stopRequested {false};
    QHash<QString, AircraftState> m_aircraft;
};

AdsbReceiver::AdsbReceiver(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("ADS-B 未启动"))
    , m_deviceCount(static_cast<int>(rtlsdr_get_device_count()))
    , m_mapCenterLatitude(envDouble("RTLSDR_PANEL_LAT", 31.2304))
    , m_mapCenterLongitude(envDouble("RTLSDR_PANEL_LON", 121.4737))
    , m_autoCenter(envInt("RTLSDR_PANEL_MAP_AUTO_CENTER", 1) != 0)
{
    connect(&m_pruneTimer, &QTimer::timeout, this, [this]() {
        const int before = m_aircraftModel.aircraftCount();
        m_aircraftModel.pruneStale(120);
        if (before != m_aircraftModel.aircraftCount()) {
            emit aircraftCountChanged();
        }
    });
    m_pruneTimer.start(5000);
}

AdsbReceiver::~AdsbReceiver()
{
    stop();
    if (m_worker != nullptr) {
        m_worker->wait(2000);
    }
}

bool AdsbReceiver::running() const
{
    return m_running;
}

QString AdsbReceiver::status() const
{
    return m_status;
}

int AdsbReceiver::deviceCount() const
{
    return m_deviceCount;
}

qulonglong AdsbReceiver::totalCandidates() const
{
    return m_totalCandidates;
}

qulonglong AdsbReceiver::validMessages() const
{
    return m_validMessages;
}

int AdsbReceiver::aircraftCount() const
{
    return m_aircraftModel.aircraftCount();
}

QObject *AdsbReceiver::aircraftModel()
{
    return &m_aircraftModel;
}

double AdsbReceiver::mapCenterLatitude() const
{
    return m_mapCenterLatitude;
}

double AdsbReceiver::mapCenterLongitude() const
{
    return m_mapCenterLongitude;
}

void AdsbReceiver::start()
{
    if (m_worker != nullptr) {
        return;
    }

    setCounters(0, 0);
    setStatus(QStringLiteral("正在启动 ADS-B 接收机"));
    m_worker = new RtlAdsbWorker;
    connect(m_worker, &RtlAdsbWorker::statusReady, this, &AdsbReceiver::setStatus);
    connect(m_worker, &RtlAdsbWorker::deviceCountReady, this, &AdsbReceiver::setDeviceCount);
    connect(m_worker, &RtlAdsbWorker::countersReady, this, &AdsbReceiver::setCounters);
    connect(m_worker, &RtlAdsbWorker::aircraftReady, this, &AdsbReceiver::updateAircraft);
    connect(m_worker, &QThread::finished, this, [this]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        setRunning(false);
    });

    setRunning(true);
    m_worker->start();
}

void AdsbReceiver::stop()
{
    if (m_worker == nullptr) {
        return;
    }

    setStatus(QStringLiteral("正在停止 ADS-B 接收机"));
    m_worker->requestStop();
}

void AdsbReceiver::clearAircraft()
{
    m_aircraftModel.clear();
    m_hasLiveCenter = false;
    emit aircraftCountChanged();
}

void AdsbReceiver::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void AdsbReceiver::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

void AdsbReceiver::setDeviceCount(int count)
{
    if (m_deviceCount == count) {
        return;
    }

    m_deviceCount = count;
    emit deviceCountChanged();
}

void AdsbReceiver::setCounters(qulonglong totalCandidates, qulonglong validMessages)
{
    if (m_totalCandidates == totalCandidates && m_validMessages == validMessages) {
        return;
    }

    m_totalCandidates = totalCandidates;
    m_validMessages = validMessages;
    emit countersChanged();
}

void AdsbReceiver::updateAircraft(const QVariantMap &aircraft)
{
    const int before = m_aircraftModel.aircraftCount();
    m_aircraftModel.updateAircraft(aircraft);
    if (before != m_aircraftModel.aircraftCount()) {
        emit aircraftCountChanged();
    }

    if (m_autoCenter && aircraft.value(QStringLiteral("hasPosition")).toBool()) {
        const double latitude = aircraft.value(QStringLiteral("latitude")).toDouble();
        const double longitude = aircraft.value(QStringLiteral("longitude")).toDouble();
        if (!m_hasLiveCenter) {
            m_hasLiveCenter = true;
            setMapCenter(latitude, longitude);
        }
    }
}

void AdsbReceiver::setMapCenter(double latitude, double longitude)
{
    if (qFuzzyCompare(m_mapCenterLatitude, latitude) && qFuzzyCompare(m_mapCenterLongitude, longitude)) {
        return;
    }

    m_mapCenterLatitude = latitude;
    m_mapCenterLongitude = longitude;
    emit mapCenterChanged();
}

#include "AdsbReceiver.moc"
