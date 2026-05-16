// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * D-Bus service for kTile: open KCM settings and show the region picker overlay.
 * Global shortcuts are registered by the KWin script; it invokes these methods via callDBus.
 */

#include "regionpickercontroller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QObject>
#include <QScreen>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QEvent>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

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

class PickerEscapeFilter : public QObject
{
    Q_OBJECT

public:
    PickerEscapeFilter(RegionPickerController *controller, QQuickWindow *window, QObject *parent = nullptr)
        : QObject(parent)
        , m_controller(controller)
        , m_window(window)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched != m_window || !m_window || !m_window->isVisible()) {
            return false;
        }
        if (event->type() != QEvent::KeyPress) {
            return false;
        }
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape || keyEvent->key() == Qt::Key_Cancel) {
            m_controller->closePicker();
            return true;
        }
        return false;
    }

private:
    RegionPickerController *m_controller = nullptr;
    QPointer<QQuickWindow> m_window;
};

namespace
{
void focusEscapeTrap(QQuickWindow *window)
{
    if (!window) {
        return;
    }
    QObject *trap = window->findChild<QObject *>(QStringLiteral("ktileEscapeTrap"));
    if (trap) {
        QMetaObject::invokeMethod(trap, "forceActiveFocus", Qt::DirectConnection,
                                 Q_ARG(Qt::FocusReason, Qt::OtherFocusReason));
    }
}

void resetPickerShellGeometry(QQuickWindow *window)
{
    if (!window) {
        return;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }
    const QRect available = screen->availableGeometry();
    window->setMinimumSize(QSize(0, 0));
    window->setMaximumSize(QSize(16777215, 16777215));
    window->setGeometry(available);
}
}

class RegionPickerHost : public QObject
{
    Q_OBJECT

public:
    explicit RegionPickerHost(QQmlApplicationEngine *engine, QObject *parent = nullptr)
        : QObject(parent)
        , m_engine(engine)
        , m_controller(new RegionPickerController(this))
    {
        connect(m_controller, &RegionPickerController::requestClose, this, &RegionPickerHost::hidePicker);
        RegionPickerController::purgeStaleClosePickerEscape();
    }

    bool isPickerVisible() const
    {
        return m_window && m_window->isVisible();
    }

    void showPicker()
    {
        if (!m_engine) {
            return;
        }

        m_controller->reloadFromConfig();

        if (!m_window) {
            m_engine->rootContext()->setContextProperty(QStringLiteral("pickerController"), m_controller);
            m_engine->load(QUrl(QStringLiteral("qrc:/qml/RegionPicker.qml")));
            if (m_engine->rootObjects().isEmpty()) {
                logLine(QStringLiteral("showPicker: failed to load RegionPicker.qml"));
                return;
            }
            m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
            if (m_window) {
                m_escapeFilter = new PickerEscapeFilter(m_controller, m_window, m_window);
                m_window->installEventFilter(m_escapeFilter);
            }
        }

        if (!m_window) {
            logLine(QStringLiteral("showPicker: root object is not a window"));
            return;
        }

        resetPickerShellGeometry(m_window);
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
        focusEscapeTrap(m_window);
    }

    void hidePicker()
    {
        if (m_window) {
            m_window->hide();
        }
    }

private:
    QQmlApplicationEngine *m_engine = nullptr;
    QPointer<RegionPickerController> m_controller;
    QPointer<QQuickWindow> m_window;
    QPointer<QObject> m_escapeFilter;
};

class KTileAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ktile.KTile")

public:
    explicit KTileAdaptor(RegionPickerHost *pickerHost, QObject *parent)
        : QDBusAbstractAdaptor(parent)
        , m_pickerHost(pickerHost)
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

    void showRegionPicker()
    {
        logLine(QStringLiteral("DBus showRegionPicker() invoked"));
        if (m_pickerHost) {
            QMetaObject::invokeMethod(m_pickerHost, &RegionPickerHost::showPicker, Qt::QueuedConnection);
        }
    }

    void closeRegionPicker()
    {
        logLine(QStringLiteral("DBus closeRegionPicker() invoked"));
        if (m_pickerHost && m_pickerHost->isPickerVisible()) {
            QMetaObject::invokeMethod(m_pickerHost, &RegionPickerHost::hidePicker, Qt::QueuedConnection);
        }
    }

private:
    RegionPickerHost *m_pickerHost = nullptr;
};
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("org.kde.ktile"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    logLine(QStringLiteral("helper start pid=%1").arg(QCoreApplication::applicationPid()));

    QQmlApplicationEngine engine;
    RegionPickerHost pickerHost(&engine);

    QObject root;
    new KTileAdaptor(&pickerHost, &root);

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
