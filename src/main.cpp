#include "AppLauncher.h"
#include "am/AmReceiver.h"
#include "adsb/AdsbReceiver.h"
#include "fm/FmReceiver.h"
#include "record/RecordReceiver.h"
#include "waterfall/WaterfallReceiver.h"

#include <QGuiApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>

namespace {
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
                                           + QByteArrayLiteral(" (ADS-B OSM map)"));
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
    AmReceiver amReceiver;
    AdsbReceiver adsbReceiver;
    FmReceiver fmReceiver;
    RecordReceiver recordReceiver;
    WaterfallReceiver waterfallReceiver;

    PanelNetworkAccessManagerFactory networkFactory;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    engine.rootContext()->setContextProperty(QStringLiteral("appLauncher"), &launcher);
    engine.rootContext()->setContextProperty(QStringLiteral("amReceiver"), &amReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("adsbReceiver"), &adsbReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("fmReceiver"), &fmReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("recordReceiver"), &recordReceiver);
    engine.rootContext()->setContextProperty(QStringLiteral("waterfallReceiver"), &waterfallReceiver);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
