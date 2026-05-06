#pragma once

#include <QObject>
#include <QString>

class RtlRecordWorker;

class RecordReceiver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString tunerName READ tunerName NOTIFY tunerInfoChanged)
    Q_PROPERTY(double centerFrequencyHz READ centerFrequencyHz WRITE setCenterFrequencyHz NOTIFY centerFrequencyHzChanged)
    Q_PROPERTY(double sampleRateHz READ sampleRateHz WRITE setSampleRateHz NOTIFY sampleRateHzChanged)
    Q_PROPERTY(double minFrequencyHz READ minFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(double maxFrequencyHz READ maxFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(QString outputFilePath READ outputFilePath WRITE setOutputFilePath NOTIFY outputFilePathChanged)
    Q_PROPERTY(QString activeFilePath READ activeFilePath NOTIFY savedLocationChanged)
    Q_PROPERTY(QString lastSavedPath READ lastSavedPath NOTIFY savedLocationChanged)
    Q_PROPERTY(QString savedLocation READ savedLocation NOTIFY savedLocationChanged)
    Q_PROPERTY(qulonglong bytesWritten READ bytesWritten NOTIFY recordingStatsChanged)
    Q_PROPERTY(double elapsedSeconds READ elapsedSeconds NOTIFY recordingStatsChanged)

public:
    explicit RecordReceiver(QObject *parent = nullptr);
    ~RecordReceiver() override;

    bool running() const;
    QString status() const;
    int deviceCount() const;
    QString tunerName() const;
    double centerFrequencyHz() const;
    double sampleRateHz() const;
    double minFrequencyHz() const;
    double maxFrequencyHz() const;
    QString outputFilePath() const;
    QString activeFilePath() const;
    QString lastSavedPath() const;
    QString savedLocation() const;
    qulonglong bytesWritten() const;
    double elapsedSeconds() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void tuneMHz(double mhz);
    Q_INVOKABLE void stepMHz(double deltaMhz);
    Q_INVOKABLE void resetOutputFilePath();

public slots:
    void setCenterFrequencyHz(double frequencyHz);
    void setSampleRateHz(double sampleRateHz);
    void setOutputFilePath(const QString &filePath);

signals:
    void runningChanged();
    void statusChanged();
    void deviceCountChanged();
    void tunerInfoChanged();
    void centerFrequencyHzChanged();
    void sampleRateHzChanged();
    void outputFilePathChanged();
    void savedLocationChanged();
    void recordingStatsChanged();

private:
    void setRunning(bool running);
    void setStatus(const QString &status);
    void setDeviceCount(int count);
    void setTunerInfo(const QString &name, double minFrequencyHz, double maxFrequencyHz);
    void setCenterFrequencyFromWorker(double frequencyHz);
    void setRecordingStats(qulonglong bytesWritten, double elapsedSeconds);
    void finishRecording(qulonglong bytesWritten, double elapsedSeconds, const QString &filePath);
    double clampedFrequency(double frequencyHz) const;
    bool frequencyInRange(double frequencyHz) const;
    QString generatedOutputFilePath() const;
    QString resolvedOutputFilePath();
    QString uniqueFilePath(const QString &filePath) const;

    RtlRecordWorker *m_worker = nullptr;
    bool m_running = false;
    bool m_customOutputFilePath = false;
    QString m_status;
    int m_deviceCount = 0;
    QString m_tunerName;
    double m_centerFrequencyHz = 100000000.0;
    double m_sampleRateHz = 2400000.0;
    double m_minFrequencyHz = 24000000.0;
    double m_maxFrequencyHz = 1766000000.0;
    QString m_outputFilePath;
    QString m_activeFilePath;
    QString m_lastSavedPath;
    qulonglong m_bytesWritten = 0;
    double m_elapsedSeconds = 0.0;
};
