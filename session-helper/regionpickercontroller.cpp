// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#include "regionpickercontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>

namespace
{
constexpr char kClosePickerActionName[] = "kTile: Close region picker";

QStringList globalActionId(const QString &actionName)
{
    QDBusInterface kga(QStringLiteral("org.kde.kglobalaccel"),
                       QStringLiteral("/kglobalaccel"),
                       QStringLiteral("org.kde.KGlobalAccel"),
                       QDBusConnection::sessionBus());
    if (!kga.isValid()) {
        return {};
    }
    const QDBusReply<QList<QStringList>> reply =
        kga.call(QStringLiteral("allActionsForComponent"), QStringList{QStringLiteral("kwin")});
    if (!reply.isValid()) {
        return {};
    }
    for (const QStringList &actionId : reply.value()) {
        if (actionId.size() >= 2 && actionId.at(1) == actionName) {
            return actionId;
        }
    }
    return {};
}

QString kwinrcPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kwinrc");
}

bool parseRegionPercents(const QString &spec, double *x, double *y, double *w, double *h)
{
    const QStringList parts = spec.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                                  Qt::SkipEmptyParts);
    if (parts.size() < 4) {
        return false;
    }
    bool ok = false;
    const double px = parts.at(0).toDouble(&ok);
    if (!ok) {
        return false;
    }
    const double py = parts.at(1).toDouble(&ok);
    if (!ok) {
        return false;
    }
    const double pw = parts.at(2).toDouble(&ok);
    if (!ok || pw <= 0) {
        return false;
    }
    const double ph = parts.at(3).toDouble(&ok);
    if (!ok || ph <= 0) {
        return false;
    }
    *x = px;
    *y = py;
    *w = pw;
    *h = ph;
    return true;
}

}

RegionPickerController::RegionPickerController(QObject *parent)
    : QObject(parent)
{
    m_autoCloseTimer.setSingleShot(true);
    connect(&m_autoCloseTimer, &QTimer::timeout, this, &RegionPickerController::closePicker);
}

void RegionPickerController::purgeStaleClosePickerEscape()
{
    const QString actionName = QString::fromLatin1(kClosePickerActionName);

    const QString shortcutsPath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kglobalshortcutsrc");
    QSettings shortcutsCfg(shortcutsPath, QSettings::IniFormat);
    shortcutsCfg.beginGroup(QStringLiteral("kwin"));
    shortcutsCfg.remove(actionName);
    shortcutsCfg.endGroup();
    shortcutsCfg.sync();

    QDBusInterface kga(QStringLiteral("org.kde.kglobalaccel"),
                       QStringLiteral("/kglobalaccel"),
                       QStringLiteral("org.kde.KGlobalAccel"),
                       QDBusConnection::sessionBus());
    if (!kga.isValid()) {
        return;
    }

    kga.call(QStringLiteral("unregister"), QStringLiteral("kwin"), actionName);

    const QStringList actionId = globalActionId(actionName);
    if (actionId.isEmpty()) {
        return;
    }

    kga.call(QStringLiteral("setForeignShortcut"),
             QVariant::fromValue(actionId),
             QVariant::fromValue(QList<int>()));
    kga.call(QStringLiteral("setInactive"), QVariant::fromValue(actionId));
}

QVariantList RegionPickerController::regions() const
{
    QVariantList list;
    list.reserve(m_regions.size());
    for (const RegionPreview &region : m_regions) {
        QVariantMap entry;
        entry.insert(QStringLiteral("index"), region.index);
        entry.insert(QStringLiteral("boxX"), region.x);
        entry.insert(QStringLiteral("boxY"), region.y);
        entry.insert(QStringLiteral("boxWidth"), region.w);
        entry.insert(QStringLiteral("boxHeight"), region.h);
        list.append(entry);
    }
    return list;
}

qreal RegionPickerController::overlayOpacity() const
{
    return m_overlayOpacity;
}

bool RegionPickerController::showPickerHeader() const
{
    return m_showPickerHeader;
}

int RegionPickerController::autoCloseSeconds() const
{
    return m_autoCloseSeconds;
}

void RegionPickerController::reloadFromConfig()
{
    m_regions.clear();

    QSettings settings(kwinrcPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Script-org.kde.ktile"));
    const qreal opacity = settings.value(QStringLiteral("regionPickerOverlayOpacity"), 0.30).toDouble();
    m_overlayOpacity = std::clamp(opacity, 0.0, 1.0);
    m_showPickerHeader = settings.value(QStringLiteral("regionPickerShowHeader"), true).toBool();
    m_autoCloseSeconds =
        std::clamp(settings.value(QStringLiteral("regionPickerAutoCloseSeconds"), 10).toInt(), 0, 300);
    int count = settings.value(QStringLiteral("regionCount"), 1).toInt();
    count = std::clamp(count, 1, 32);

    m_regions.reserve(count);
    for (int i = 1; i <= count; ++i) {
        const QString spec =
            settings.value(QStringLiteral("region%1").arg(i), QStringLiteral("0 0 50 50")).toString();
        RegionPreview preview;
        preview.index = i;
        if (!parseRegionPercents(spec, &preview.x, &preview.y, &preview.w, &preview.h)) {
            continue;
        }
        m_regions.push_back(preview);
    }
    settings.endGroup();

    Q_EMIT regionsChanged();
    Q_EMIT overlayOpacityChanged();
    Q_EMIT showPickerHeaderChanged();
    Q_EMIT autoCloseSecondsChanged();
}

void RegionPickerController::beginAutoCloseTimer()
{
    m_autoCloseTimer.stop();
    if (m_autoCloseSeconds <= 0) {
        return;
    }
    m_autoCloseTimer.start(m_autoCloseSeconds * 1000);
}

void RegionPickerController::cancelAutoCloseTimer()
{
    m_autoCloseTimer.stop();
}

void RegionPickerController::invokeRegionShortcut(int oneBasedIndex)
{
    if (oneBasedIndex < 1) {
        return;
    }

    QDBusInterface component(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kwin"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
    if (!component.isValid()) {
        return;
    }

    const QString actionName = QStringLiteral("kTile: region %1").arg(oneBasedIndex);
    component.call(QStringLiteral("invokeShortcut"), actionName);
}

void RegionPickerController::snapToRegion(int oneBasedIndex)
{
    if (oneBasedIndex < 1) {
        return;
    }

    cancelAutoCloseTimer();
    // Hide first so KWin's activeWindow is the user's window again, not this picker.
    Q_EMIT requestClose();
    QTimer::singleShot(120, this, [this, oneBasedIndex]() { invokeRegionShortcut(oneBasedIndex); });
}

void RegionPickerController::closePicker()
{
    cancelAutoCloseTimer();
    Q_EMIT requestClose();
}

void RegionPickerController::invokeDrawRegionShortcut()
{
    QDBusInterface component(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kwin"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
    if (!component.isValid()) {
        return;
    }
    component.call(QStringLiteral("invokeShortcut"), QStringLiteral("kTile: Draw region on screen"));
}

void RegionPickerController::openSettings()
{
    cancelAutoCloseTimer();
    QDBusInterface iface(QStringLiteral("org.kde.ktile"),
                         QStringLiteral("/KTile"),
                         QStringLiteral("org.kde.ktile.KTile"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        iface.call(QStringLiteral("open"));
    }
    Q_EMIT requestClose();
}

void RegionPickerController::openDrawRegion()
{
    cancelAutoCloseTimer();
    // Hide first so KWin's activeWindow is the user's window again, not this picker.
    Q_EMIT requestClose();
    QTimer::singleShot(120, this, [this]() { invokeDrawRegionShortcut(); });
}
