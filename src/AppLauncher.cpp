#include "AppLauncher.h"

#include <QProcess>
#include <QStringList>

AppLauncher::AppLauncher(QObject *parent)
    : QObject(parent)
    , m_lastMessage(QStringLiteral("系统就绪"))
{
}

QString AppLauncher::lastMessage() const
{
    return m_lastMessage;
}

QString AppLauncher::platformHint() const
{
#if defined(RTLSDR_PANEL_TARGET_LABEL)
    return QStringLiteral(RTLSDR_PANEL_TARGET_LABEL);
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("ARM64 / Embedded Linux");
#elif defined(Q_PROCESSOR_ARM)
    return QStringLiteral("ARM / Embedded Linux");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("Linux Desktop");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Windows Desktop");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macOS Desktop");
#else
    return QStringLiteral("Qt Runtime");
#endif
}

QString AppLauncher::commandFor(const QString &appId) const
{
    const QByteArray envName = envNameFor(appId).toLocal8Bit();
    return QString::fromLocal8Bit(qgetenv(envName.constData())).trimmed();
}

bool AppLauncher::launch(const QString &appId)
{
    const QString command = commandFor(appId);
    const QString envName = envNameFor(appId);
    const QString appName = displayNameFor(appId);

    if (command.isEmpty()) {
        const QString message = QStringLiteral("%1 未配置启动命令：请设置环境变量 %2")
                                    .arg(appName, envName);
        setLastMessage(message);
        emit launchFailed(appId, message);
        return false;
    }

#if defined(Q_OS_WIN)
    const bool started = QProcess::startDetached(QStringLiteral("cmd.exe"),
                                                 QStringList() << QStringLiteral("/C") << command);
#else
    const bool started = QProcess::startDetached(QStringLiteral("/bin/sh"),
                                                 QStringList() << QStringLiteral("-lc") << command);
#endif

    if (!started) {
        const QString message = QStringLiteral("%1 启动失败").arg(appName);
        setLastMessage(message);
        emit launchFailed(appId, message);
        return false;
    }

    setLastMessage(QStringLiteral("已启动：%1").arg(appName));
    emit launched(appId);
    return true;
}

void AppLauncher::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

QString AppLauncher::envNameFor(const QString &appId)
{
    if (appId == QStringLiteral("adsb")) {
        return QStringLiteral("RTLSDR_PANEL_ADSB_CMD");
    }
    if (appId == QStringLiteral("am")) {
        return QStringLiteral("RTLSDR_PANEL_AM_CMD");
    }
    if (appId == QStringLiteral("fm")) {
        return QStringLiteral("RTLSDR_PANEL_FM_CMD");
    }
    if (appId == QStringLiteral("waterfall")) {
        return QStringLiteral("RTLSDR_PANEL_WATERFALL_CMD");
    }
    if (appId == QStringLiteral("record")) {
        return QStringLiteral("RTLSDR_PANEL_RECORD_CMD");
    }
    if (appId == QStringLiteral("sensors")) {
        return QStringLiteral("RTLSDR_PANEL_SENSORS_CMD");
    }

    return QStringLiteral("RTLSDR_PANEL_%1_CMD").arg(appId.toUpper());
}

QString AppLauncher::displayNameFor(const QString &appId)
{
    if (appId == QStringLiteral("adsb")) {
        return QStringLiteral("ADS-B 飞机识别");
    }
    if (appId == QStringLiteral("am")) {
        return QStringLiteral("AM 接收");
    }
    if (appId == QStringLiteral("fm")) {
        return QStringLiteral("FM 接收");
    }
    if (appId == QStringLiteral("waterfall")) {
        return QStringLiteral("频谱瀑布图");
    }
    if (appId == QStringLiteral("record")) {
        return QStringLiteral("射频信号录制");
    }
    if (appId == QStringLiteral("sensors")) {
        return QStringLiteral("433/315 传感器解码");
    }

    return appId;
}
