#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class RtlFmWorker;

class FmReceiver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString tunerName READ tunerName NOTIFY tunerInfoChanged)
    Q_PROPERTY(double centerFrequencyHz READ centerFrequencyHz WRITE setCenterFrequencyHz NOTIFY centerFrequencyHzChanged)
    Q_PROPERTY(double sampleRateHz READ sampleRateHz NOTIFY sampleRateHzChanged)
    Q_PROPERTY(double audioRateHz READ audioRateHz NOTIFY audioRateHzChanged)
    Q_PROPERTY(double minFrequencyHz READ minFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(double maxFrequencyHz READ maxFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(int fftSize READ fftSize CONSTANT)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString modeLabel READ modeLabel NOTIFY modeChanged)
    Q_PROPERTY(double audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVariantList audioOutputDevices READ audioOutputDevices NOTIFY audioOutputDevicesChanged)
    Q_PROPERTY(QString audioOutputDeviceId READ audioOutputDeviceId WRITE setAudioOutputDeviceId NOTIFY audioOutputDeviceChanged)
    Q_PROPERTY(QString audioOutputDeviceLabel READ audioOutputDeviceLabel NOTIFY audioOutputDeviceChanged)

public:
    explicit FmReceiver(QObject *parent = nullptr);
    ~FmReceiver() override;

    bool running() const;
    QString status() const;
    int deviceCount() const;
    QString tunerName() const;
    double centerFrequencyHz() const;
    double sampleRateHz() const;
    double audioRateHz() const;
    double minFrequencyHz() const;
    double maxFrequencyHz() const;
    int fftSize() const;
    QString mode() const;
    QString modeLabel() const;
    double audioLevel() const;
    double volume() const;
    QVariantList audioOutputDevices() const;
    QString audioOutputDeviceId() const;
    QString audioOutputDeviceLabel() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void tuneMHz(double mhz);
    Q_INVOKABLE void stepMHz(double deltaMhz);
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void setAudioOutputDeviceId(const QString &deviceId);
    Q_INVOKABLE void refreshAudioOutputDevices();

public slots:
    void setCenterFrequencyHz(double frequencyHz);

signals:
    void runningChanged();
    void statusChanged();
    void deviceCountChanged();
    void tunerInfoChanged();
    void centerFrequencyHzChanged();
    void sampleRateHzChanged();
    void audioRateHzChanged();
    void modeChanged();
    void audioLevelChanged();
    void volumeChanged();
    void audioOutputDevicesChanged();
    void audioOutputDeviceChanged();
    void spectrumReady(const QVariantList &levels, double leftFrequencyHz, double rightFrequencyHz);

private:
    void setRunning(bool running);
    void setStatus(const QString &status);
    void setDeviceCount(int count);
    void setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz);
    void setCenterFrequencyFromWorker(double frequencyHz);
    void setAudioLevel(double level);
    double clampedFrequency(double frequencyHz) const;
    bool frequencyInRange(double frequencyHz) const;
    double sampleRateForMode(const QString &mode) const;
    QString normalizedMode(const QString &mode) const;
    bool ensureSelectedAudioDeviceListed();

    RtlFmWorker *m_worker = nullptr;
    bool m_running = false;
    bool m_restartAfterStop = false;
    QString m_status;
    int m_deviceCount = 0;
    QString m_tunerName;
    double m_centerFrequencyHz = 100700000.0;
    double m_audioRateHz = 48000.0;
    double m_minFrequencyHz = 24000000.0;
    double m_maxFrequencyHz = 1766000000.0;
    int m_fftSize = 512;
    QString m_mode;
    double m_audioLevel = 0.0;
    double m_volume = 0.80;
    QVariantList m_audioOutputDevices;
    QString m_audioOutputDeviceId;
};
