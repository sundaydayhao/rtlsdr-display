#pragma once

#include <QObject>
#include <QString>

class RtlAmWorker;

class AmReceiver : public QObject
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
    Q_PROPERTY(double audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit AmReceiver(QObject *parent = nullptr);
    ~AmReceiver() override;

    bool running() const;
    QString status() const;
    int deviceCount() const;
    QString tunerName() const;
    double centerFrequencyHz() const;
    double sampleRateHz() const;
    double audioRateHz() const;
    double minFrequencyHz() const;
    double maxFrequencyHz() const;
    double audioLevel() const;
    double volume() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void tuneMHz(double mhz);
    Q_INVOKABLE void stepMHz(double deltaMhz);
    Q_INVOKABLE void setVolume(double volume);

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
    void audioLevelChanged();
    void volumeChanged();

private:
    void setRunning(bool running);
    void setStatus(const QString &status);
    void setDeviceCount(int count);
    void setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz);
    void setCenterFrequencyFromWorker(double frequencyHz);
    void setAudioLevel(double level);
    double clampedFrequency(double frequencyHz) const;
    bool frequencyInRange(double frequencyHz) const;

    RtlAmWorker *m_worker = nullptr;
    bool m_running = false;
    QString m_status;
    int m_deviceCount = 0;
    QString m_tunerName;
    double m_centerFrequencyHz = 118000000.0;
    double m_sampleRateHz = 240000.0;
    double m_audioRateHz = 48000.0;
    double m_minFrequencyHz = 24000000.0;
    double m_maxFrequencyHz = 1766000000.0;
    double m_audioLevel = 0.0;
    double m_volume = 0.80;
};
