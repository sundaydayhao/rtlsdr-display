#include "AppLauncher.h"
#include "adsb/AdsbReceiver.h"
#include "fm/FmReceiver.h"
#include "record/RecordReceiver.h"
#include "sensors/SensorReceiver.h"
#include "waterfall/WaterfallReceiver.h"
#include "scanner/ScannerReceiver.h"

#include <QByteArray>
#include <QFile>
#include <QGuiApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQmlContext>
#include <QString>
#include <QStandardPaths>
#include <QUrl>

namespace {
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QByteArray data = file.readAll();
    if (data.isEmpty()) {
        return {};
    }

    data.replace('\0', ' ');
    return QString::fromUtf8(data).trimmed();
}

bool useEmbeddedWindowMode()
{
    const QString target = QStringLiteral(RTLSDR_PANEL_TARGET);
    if (target == QStringLiteral("rk3566") || target == QStringLiteral("raspberrypi")) {
        return true;
    }
    if (target == QStringLiteral("desktop")) {
        return false;
    }

    const QString deviceDescription = (
        readTextFile(QStringLiteral("/sys/firmware/devicetree/base/model"))
        + QStringLiteral(" ")
        + readTextFile(QStringLiteral("/sys/firmware/devicetree/base/compatible"))
        + QStringLiteral(" ")
        + readTextFile(QStringLiteral("/proc/device-tree/model"))
        + QStringLiteral(" ")
        + readTextFile(QStringLiteral("/proc/device-tree/compatible"))).toLower();

    if (deviceDescription.contains(QStringLiteral("raspberry pi"))
            || deviceDescription.contains(QStringLiteral("raspberrypi"))
            || deviceDescription.contains(QStringLiteral("rk3566"))) {
        return true;
    }

    const QByteArray platform = qgetenv("QT_QPA_PLATFORM").toLower();
    return platform == QByteArrayLiteral("eglfs") || platform == QByteArrayLiteral("linuxfb");
}

class PanelNetworkAccessManager final : public QNetworkAccessManager
{
public:
    explicit PanelNetworkAccessManager(QObject *parent = nullptr)
        : QNetworkAccessManager(parent)
    {
        auto *cache = new QNetworkDiskCache(this);
        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (cachePath.isEmpty()) {
            cachePath = QDir::tempPath() + QStringLiteral("/rtlsdr-panel");
        }
        cachePath += QStringLiteral("/map-tiles");
        QDir().mkpath(cachePath);
        cache->setCacheDirectory(cachePath);
        cache->setMaximumCacheSize(128 * 1024 * 1024);
        setCache(cache);
    }

protected:
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData = nullptr) override
    {
        QNetworkRequest taggedRequest(request);
        if (!taggedRequest.hasRawHeader("User-Agent")) {
            taggedRequest.setRawHeader("User-Agent",
                                       QByteArrayLiteral("rtlsdr-panel/")
                                           + QByteArrayLiteral(APP_VERSION)
                                           + QByteArrayLiteral(" (ADS-B map)"));
        }
        return QNetworkAccessManager::createRequest(operation, taggedRequest, outgoingData);
    }
};

class PanelNetworkAccessManagerFactory final : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        return new PanelNetworkAccessManager(parent);
    }
};
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("rtlsdr"));
    app.setApplicationName(QStringLiteral("RTL-SDR Panel"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    AppLauncher launcher;
    AdsbReceiver adsbReceiver;
    FmReceiver fmReceiver;
    RecordReceiver recordReceiver;
    SensorReceiver sensorReceiver;
    WaterfallReceiver waterfallReceiver;
    ScannerReceiver   scannerReceiver;

    PanelNetworkAccessManagerFactory networkFactory;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    engine.rootContext()->setContextProperty(QStringLiteral("appLauncher"), &launcher);
    engine.rootContext()->setContextProperty(QStringLiteral("adsbReceiver"), &adsbReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("fmReceiver"), &fmReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("recordReceiver"), &recordReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("sensorReceiver"), &sensorReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("waterfallReceiver"), &waterfallReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("scannerReceiver"), &scannerReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("panelEmbeddedUi"), useEmbeddedWindowMode());
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
