// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * D-Bus service for opening the kTile KCM. The global shortcut is registered by the KWin script
 * (same component as region shortcuts → Window Management in Shortcuts). Pressing the shortcut
 * runs this process's open() method; KWin calls it via callDBus (reliable) instead of klauncher.
 */

#include <QCoreApplication>
#include <QDateTime>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
QString logPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.cache");
    }
    QDir().mkpath(base);
    return base + QStringLiteral("/ktile-session-helper.log");
}

void logLine(const QString &line)
{
    QFile f(logPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << line << "\n";
}

QProcessEnvironment environmentFromSessionProcess()
{
    const QStringList preferred = {
        QStringLiteral("plasmashell"),
        QStringLiteral("kwin_wayland"),
        QStringLiteral("kwin_x11"),
        QStringLiteral("ksmserver")
    };
    const QDir proc(QStringLiteral("/proc"));
    const QStringList pids = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &want : preferred) {
        for (const QString &pidText : pids) {
            bool ok = false;
            const int pid = pidText.toInt(&ok);
            if (!ok || pid <= 0) {
                continue;
            }
            const QString commPath = QStringLiteral("/proc/%1/comm").arg(pid);
            QFile commFile(commPath);
            if (!commFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QString comm = QString::fromUtf8(commFile.readAll()).trimmed();
            if (comm != want) {
                continue;
            }
            const QString envPath = QStringLiteral("/proc/%1/environ").arg(pid);
            QFile envFile(envPath);
            if (!envFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray raw = envFile.readAll();
            if (raw.isEmpty()) {
                continue;
            }
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            const QList<QByteArray> parts = raw.split('\0');
            for (const QByteArray &part : parts) {
                const int eq = part.indexOf('=');
                if (eq <= 0) {
                    continue;
                }
                env.insert(QString::fromUtf8(part.left(eq)), QString::fromUtf8(part.mid(eq + 1)));
            }
            return env;
        }
    }
    return QProcessEnvironment::systemEnvironment();
}

bool startDetachedWithSessionEnv(const QString &program, const QStringList &args)
{
    if (program.isEmpty()) {
        logLine(QStringLiteral("startDetachedWithSessionEnv: empty program"));
        return false;
    }
    logLine(QStringLiteral("startDetachedWithSessionEnv: program=%1 args=%2")
                .arg(program, args.join(QLatin1Char(' '))));
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    const QProcessEnvironment env = environmentFromSessionProcess();
    proc.setProcessEnvironment(env);
    const bool ok = proc.startDetached();
    logLine(QStringLiteral("startDetachedWithSessionEnv: result=%1 DISPLAY=%2 WAYLAND_DISPLAY=%3 XDG_RUNTIME_DIR=%4")
                .arg(ok ? QStringLiteral("ok") : QStringLiteral("fail"),
                     env.value(QStringLiteral("DISPLAY")),
                     env.value(QStringLiteral("WAYLAND_DISPLAY")),
                     env.value(QStringLiteral("XDG_RUNTIME_DIR"))));
    return ok;
}
}

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
        logLine(QStringLiteral("DBus open() invoked"));
        QString kcmshell = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
        if (kcmshell.isEmpty()) {
            static const QString kDefaultKcmShell = QStringLiteral("/usr/bin/kcmshell6");
            if (QFileInfo::exists(kDefaultKcmShell)) {
                kcmshell = kDefaultKcmShell;
            }
        }
        if (startDetachedWithSessionEnv(kcmshell, {QStringLiteral("kcm_ktile")})) {
            return;
        }

        QString systemsettings = QStandardPaths::findExecutable(QStringLiteral("systemsettings"));
        if (systemsettings.isEmpty()) {
            static const QString kDefaultSystemSettings = QStringLiteral("/usr/bin/systemsettings");
            if (QFileInfo::exists(kDefaultSystemSettings)) {
                systemsettings = kDefaultSystemSettings;
            }
        }
        if (startDetachedWithSessionEnv(systemsettings, {QStringLiteral("kcm_ktile")})) {
            return;
        }
        qWarning() << "ktile-session-helper: failed to launch kcm_ktile via kcmshell6/systemsettings";
        logLine(QStringLiteral("open(): launch failed via kcmshell6 and systemsettings"));
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("org.kde.ktile"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    logLine(QStringLiteral("helper start pid=%1").arg(QCoreApplication::applicationPid()));

    QObject root;
    new KTileAdaptor(&root);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.kde.ktile"))) {
        qWarning() << "ktile-session-helper: could not own org.kde.ktile on the session bus (already running?)";
        logLine(QStringLiteral("registerService(org.kde.ktile) failed"));
        return 1;
    }
    bus.registerObject(QStringLiteral("/KTile"), &root);
    logLine(QStringLiteral("DBus service registered: org.kde.ktile /KTile"));

    return app.exec();
}

#include "main.moc"
