#pragma once

#include "SensorEventModel.h"

#include <QObject>
#include <QTimer>

class RtlSensorWorker;

class SensorReceiver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString tunerName READ tunerName NOTIFY tunerInfoChanged)
    Q_PROPERTY(double centerFrequencyHz READ centerFrequencyHz WRITE setCenterFrequencyHz NOTIFY centerFrequencyHzChanged)
    Q_PROPERTY(double sampleRateHz READ sampleRateHz NOTIFY sampleRateHzChanged)
    Q_PROPERTY(double minFrequencyHz READ minFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(double maxFrequencyHz READ maxFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(QString bandLabel READ bandLabel NOTIFY centerFrequencyHzChanged)
    Q_PROPERTY(qulonglong totalFrames READ totalFrames NOTIFY countersChanged)
    Q_PROPERTY(qulonglong decodedMessages READ decodedMessages NOTIFY countersChanged)
    Q_PROPERTY(int sensorCount READ sensorCount NOTIFY sensorCountChanged)
    Q_PROPERTY(double signalLevel READ signalLevel NOTIFY signalLevelChanged)
    Q_PROPERTY(QObject *eventModel READ eventModel CONSTANT)

public:
    explicit SensorReceiver(QObject *parent = nullptr);
    ~SensorReceiver() override;

    bool running() const;
    QString status() const;
    int deviceCount() const;
    QString tunerName() const;
    double centerFrequencyHz() const;
    double sampleRateHz() const;
    double minFrequencyHz() const;
    double maxFrequencyHz() const;
    QString bandLabel() const;
    qulonglong totalFrames() const;
    qulonglong decodedMessages() const;
    int sensorCount() const;
    double signalLevel() const;
    QObject *eventModel();

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void tuneMHz(double mhz);
    Q_INVOKABLE void stepMHz(double deltaMhz);
    Q_INVOKABLE void setBand(const QString &band);
    Q_INVOKABLE void clearEvents();

public slots:
    void setCenterFrequencyHz(double frequencyHz);

signals:
    void runningChanged();
    void statusChanged();
    void deviceCountChanged();
    void tunerInfoChanged();
    void centerFrequencyHzChanged();
    void sampleRateHzChanged();
    void countersChanged();
    void sensorCountChanged();
    void signalLevelChanged();

private:
    void setRunning(bool running);
    void setStatus(const QString &status);
    void setDeviceCount(int count);
    void setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz);
    void setCenterFrequencyFromWorker(double frequencyHz);
    void setCounters(qulonglong totalFrames, qulonglong decodedMessages);
    void setSignalLevel(double level);
    void handleEvents(const QVariantList &events);
    double clampedFrequency(double frequencyHz) const;
    bool frequencyInRange(double frequencyHz) const;

    SensorEventModel m_eventModel;
    QTimer m_pruneTimer;
    RtlSensorWorker *m_worker = nullptr;
    bool m_running = false;
    QString m_status;
    int m_deviceCount = 0;
    QString m_tunerName;
    double m_centerFrequencyHz = 433920000.0;
    double m_sampleRateHz = 250000.0;
    double m_minFrequencyHz = 24000000.0;
    double m_maxFrequencyHz = 1766000000.0;
    qulonglong m_totalFrames = 0;
    qulonglong m_decodedMessages = 0;
    double m_signalLevel = 0.0;
};
