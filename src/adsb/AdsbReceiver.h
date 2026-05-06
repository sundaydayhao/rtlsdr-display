#pragma once

#include "AdsbAircraftModel.h"

#include <QObject>
#include <QTimer>

class RtlAdsbWorker;

class AdsbReceiver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY deviceCountChanged)
    Q_PROPERTY(qulonglong totalCandidates READ totalCandidates NOTIFY countersChanged)
    Q_PROPERTY(qulonglong validMessages READ validMessages NOTIFY countersChanged)
    Q_PROPERTY(int aircraftCount READ aircraftCount NOTIFY aircraftCountChanged)
    Q_PROPERTY(QObject *aircraftModel READ aircraftModel CONSTANT)
    Q_PROPERTY(double mapCenterLatitude READ mapCenterLatitude NOTIFY mapCenterChanged)
    Q_PROPERTY(double mapCenterLongitude READ mapCenterLongitude NOTIFY mapCenterChanged)

public:
    explicit AdsbReceiver(QObject *parent = nullptr);
    ~AdsbReceiver() override;

    bool running() const;
    QString status() const;
    int deviceCount() const;
    qulonglong totalCandidates() const;
    qulonglong validMessages() const;
    int aircraftCount() const;
    QObject *aircraftModel();
    double mapCenterLatitude() const;
    double mapCenterLongitude() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clearAircraft();

signals:
    void runningChanged();
    void statusChanged();
    void deviceCountChanged();
    void countersChanged();
    void aircraftCountChanged();
    void mapCenterChanged();

private:
    void setRunning(bool running);
    void setStatus(const QString &status);
    void setDeviceCount(int count);
    void setCounters(qulonglong totalCandidates, qulonglong validMessages);
    void updateAircraft(const QVariantMap &aircraft);
    void setMapCenter(double latitude, double longitude);

    AdsbAircraftModel m_aircraftModel;
    QTimer m_pruneTimer;
    RtlAdsbWorker *m_worker = nullptr;

    bool m_running = false;
    QString m_status;
    int m_deviceCount = 0;
    qulonglong m_totalCandidates = 0;
    qulonglong m_validMessages = 0;
    double m_mapCenterLatitude = 31.2304;
    double m_mapCenterLongitude = 121.4737;
    bool m_autoCenter = true;
    bool m_hasLiveCenter = false;
};
