#pragma once

#include <QObject>
#include <QString>

class AppLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(QString platformHint READ platformHint CONSTANT)

public:
    explicit AppLauncher(QObject *parent = nullptr);

    QString lastMessage() const;
    QString platformHint() const;

    Q_INVOKABLE QString commandFor(const QString &appId) const;
    Q_INVOKABLE bool launch(const QString &appId);

signals:
    void launched(const QString &appId);
    void launchFailed(const QString &appId, const QString &message);
    void lastMessageChanged();

private:
    void setLastMessage(const QString &message);
    static QString envNameFor(const QString &appId);
    static QString displayNameFor(const QString &appId);

    QString m_lastMessage;
};
