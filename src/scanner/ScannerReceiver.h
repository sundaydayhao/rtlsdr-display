#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class RtlScanWorker;

class ScannerReceiver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(QString tunerName READ tunerName NOTIFY tunerInfoChanged)
    Q_PROPERTY(double startFrequencyHz READ startFrequencyHz NOTIFY startFrequencyHzChanged)
    Q_PROPERTY(double stopFrequencyHz READ stopFrequencyHz NOTIFY stopFrequencyHzChanged)
    Q_PROPERTY(double sampleRateHz READ sampleRateHz NOTIFY sampleRateHzChanged)
    Q_PROPERTY(int directSampling READ directSampling WRITE setDirectSampling NOTIFY directSamplingChanged)
    Q_PROPERTY(int fftSize READ fftSize CONSTANT)
    Q_PROPERTY(int totalBins READ totalBins NOTIFY totalBinsChanged)
    Q_PROPERTY(double minFrequencyHz READ minFrequencyHz NOTIFY tunerInfoChanged)
    Q_PROPERTY(double maxFrequencyHz READ maxFrequencyHz NOTIFY tunerInfoChanged)

public:
    explicit ScannerReceiver(QObject *parent = nullptr);
    ~ScannerReceiver() override;

    bool running() const;
    QString status() const;
    int deviceCount() const;
    QString tunerName() const;
    double startFrequencyHz() const;
    double stopFrequencyHz() const;
    double sampleRateHz() const;
    int directSampling() const;
    int fftSize() const;
    int totalBins() const;
    double minFrequencyHz() const;
    double maxFrequencyHz() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setStartFrequencyHz(double hz);
    Q_INVOKABLE void setStopFrequencyHz(double hz);
    Q_INVOKABLE void setFrequencyRangeHz(double startHz, double stopHz);
    Q_INVOKABLE void setDirectSampling(int directSampling);

signals:
    void runningChanged();
    void statusChanged();
    void deviceCountChanged();
    void tunerInfoChanged();
    void startFrequencyHzChanged();
    void stopFrequencyHzChanged();
    void sampleRateHzChanged();
    void directSamplingChanged();
    void totalBinsChanged();
    void scanLineReady(const QVariantList &powers, double startHz, double stopHz);

private:
    void setRunning(bool running);
    void setStatus(const QString &status);
    void setDeviceCount(int count);
    void setTunerInfo(const QString &name, double minHz, double maxHz);
    void refreshConfiguredFrequencyRange();
    void clampConfiguredScanRange();
    void updateTotalBins();

    RtlScanWorker *m_worker = nullptr;
    bool m_running = false;
    QString m_status;
    int m_deviceCount = 0;
    QString m_tunerName;
    double m_startFrequencyHz = 88000000.0;
    double m_stopFrequencyHz  = 108000000.0;
    double m_sampleRateHz     = 2400000.0;
    int    m_directSampling   = 0;
    double m_minFrequencyHz   = 24000000.0;
    double m_maxFrequencyHz   = 1766000000.0;
    int    m_fftSize           = 512;
    int    m_totalBins         = 0;
};
