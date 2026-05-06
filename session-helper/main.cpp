// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * D-Bus service for opening the kTile KCM. The global shortcut is registered by the KWin script
 * (same component as region shortcuts → Window Management in Shortcuts). Pressing the shortcut
 * runs this process's open() method; KWin calls it via callDBus (reliable) instead of klauncher.
 */

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QObject>
#include <QProcess>

class KTileAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ktile.KTile")

public:
    explicit KTileAdaptor(QObject *parent)
        : QDBusAbstractAdaptor(parent)
    {
    }

public Q_SLOTS:
    void open()
    {
        const QString envFile =
            QDir::homePath() + QStringLiteral("/.config/plasma-workspace/env/ktile-paths.sh");
        const QString cmd =
            QStringLiteral(". \"%1\" 2>/dev/null; exec kcmshell6 kcm_ktile").arg(envFile);
        if (!QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd})) {
            qWarning() << "ktile-session-helper: failed to launch kcmshell6";
        }
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("org.kde.ktile"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));

    QObject root;
    new KTileAdaptor(&root);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.kde.ktile"))) {
        qWarning() << "ktile-session-helper: could not own org.kde.ktile on the session bus (already running?)";
        return 1;
    }
    bus.registerObject(QStringLiteral("/KTile"), &root);

    return app.exec();
}

#include "main.moc"
