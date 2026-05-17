// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#include "drawregioncontroller.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>
#include <QStandardPaths>
#include <QRect>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace
{
QString kwinrcPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kwinrc");
}

QString logFilePath()
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
    QFile f(logFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " " << line << "\n";
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

bool nearlyEqual(qreal a, qreal b)
{
    return std::abs(a - b) < 0.05;
}

}

DrawRegionController::DrawRegionController(QObject *parent)
    : QObject(parent)
{
    m_autoCloseTimer.setSingleShot(true);
    connect(&m_autoCloseTimer, &QTimer::timeout, this, &DrawRegionController::closeOverlay);
}

qreal DrawRegionController::overlayOpacity() const
{
    return m_overlayOpacity;
}

int DrawRegionController::gridColumns() const
{
    return m_gridColumns;
}

int DrawRegionController::gridRows() const
{
    return m_gridRows;
}

int DrawRegionController::gridGap() const
{
    return m_gridGap;
}

bool DrawRegionController::showGridLines() const
{
    return m_showGridLines;
}

int DrawRegionController::autoCloseSeconds() const
{
    return m_autoCloseSeconds;
}

QRect DrawRegionController::tilingBasisRect() const
{
    return m_tilingBasis;
}

void DrawRegionController::reloadFromConfig()
{
    QSettings settings(kwinrcPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Script-org.kde.ktile"));
    const qreal opacity = settings.value(QStringLiteral("drawRegionOverlayOpacity"),
                                        settings.value(QStringLiteral("regionPickerOverlayOpacity"), 0.30))
                              .toDouble();
    m_overlayOpacity = std::clamp(opacity, 0.0, 1.0);
    m_gridColumns = std::clamp(settings.value(QStringLiteral("gridColumns"), 8).toInt(), 1, 64);
    m_gridRows = std::clamp(settings.value(QStringLiteral("gridRows"), 6).toInt(), 1, 64);
    m_gridGap = std::clamp(settings.value(QStringLiteral("gridGap"), 0).toInt(), 0, 48);
    m_showGridLines = settings.value(QStringLiteral("drawRegionShowGridLines"), false).toBool();
    m_autoCloseSeconds =
        std::clamp(settings.value(QStringLiteral("drawRegionAutoCloseSeconds"), 5).toInt(), 0, 300);
    const int bx = settings.value(QStringLiteral("drawRegionTilingBasisX"), -1).toInt();
    const int by = settings.value(QStringLiteral("drawRegionTilingBasisY"), -1).toInt();
    const int bw = settings.value(QStringLiteral("drawRegionTilingBasisW"), -1).toInt();
    const int bh = settings.value(QStringLiteral("drawRegionTilingBasisH"), -1).toInt();
    if (bw > 0 && bh > 0) {
        m_tilingBasis = QRect(bx, by, bw, bh);
    }
    settings.endGroup();
    Q_EMIT overlayOpacityChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT showGridLinesChanged();
    Q_EMIT autoCloseSecondsChanged();
}

void DrawRegionController::beginAutoCloseTimer()
{
    m_autoCloseTimer.stop();
    if (m_autoCloseSeconds <= 0) {
        return;
    }
    m_autoCloseTimer.start(m_autoCloseSeconds * 1000);
}

void DrawRegionController::cancelAutoCloseTimer()
{
    m_autoCloseTimer.stop();
}

void DrawRegionController::closeOverlay()
{
    cancelAutoCloseTimer();
    Q_EMIT requestClose();
}

void DrawRegionController::setTargetWindowInternalId(const QString &internalId)
{
    m_targetInternalId = internalId.trimmed();
}

void DrawRegionController::setDrawRegionTilingBasis(int x, int y, int width, int height)
{
    if (width < 1 || height < 1) {
        m_tilingBasis = QRect();
        return;
    }
    m_tilingBasis = QRect(x, y, width, height);
    QSettings settings(kwinrcPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Script-org.kde.ktile"));
    settings.setValue(QStringLiteral("drawRegionTilingBasisX"), x);
    settings.setValue(QStringLiteral("drawRegionTilingBasisY"), y);
    settings.setValue(QStringLiteral("drawRegionTilingBasisW"), width);
    settings.setValue(QStringLiteral("drawRegionTilingBasisH"), height);
    settings.endGroup();
    settings.sync();
}

QVariantMap DrawRegionController::takePendingDrawRegionSnap()
{
    if (!m_hasPendingDrawRegionSnap) {
        return {};
    }
    m_hasPendingDrawRegionSnap = false;
    return m_pendingDrawRegionSnap;
}

void DrawRegionController::invokeDrawRegionSnapShortcut()
{
    QDBusInterface component(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kwin"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
    if (!component.isValid()) {
        logLine(QStringLiteral("invokeDrawRegionSnapShortcut: kglobalaccel component/kwin invalid"));
        return;
    }
    const QDBusMessage reply =
        component.call(QStringLiteral("invokeShortcut"), QStringLiteral("kTile: Apply drawn region"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        logLine(QStringLiteral("invokeDrawRegionSnapShortcut failed: %1").arg(reply.errorMessage()));
    } else {
        logLine(QStringLiteral("invokeDrawRegionSnapShortcut ok"));
    }
}

void DrawRegionController::invokeRegionShortcut(int oneBasedIndex)
{
    if (oneBasedIndex < 1) {
        return;
    }
    QDBusInterface component(QStringLiteral("org.kde.kglobalaccel"),
                             QStringLiteral("/component/kwin"),
                             QStringLiteral("org.kde.kglobalaccel.Component"),
                             QDBusConnection::sessionBus());
    if (!component.isValid()) {
        logLine(QStringLiteral("invokeRegionShortcut(%1): kglobalaccel invalid").arg(oneBasedIndex));
        return;
    }
    const QString actionName = QStringLiteral("kTile: region %1").arg(oneBasedIndex);
    const QDBusMessage reply = component.call(QStringLiteral("invokeShortcut"), actionName);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        logLine(QStringLiteral("invokeRegionShortcut(%1) failed: %2")
                    .arg(oneBasedIndex)
                    .arg(reply.errorMessage()));
    } else {
        logLine(QStringLiteral("invokeRegionShortcut(%1) ok").arg(oneBasedIndex));
    }
}

int DrawRegionController::matchingConfiguredRegion(qreal xPct, qreal yPct, qreal wPct, qreal hPct) const
{
    QSettings settings(kwinrcPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Script-org.kde.ktile"));
    int count = settings.value(QStringLiteral("regionCount"), 1).toInt();
    count = std::clamp(count, 1, 32);
    for (int i = 1; i <= count; ++i) {
        const QString spec =
            settings.value(QStringLiteral("region%1").arg(i), QStringLiteral("0 0 50 50")).toString();
        double rx = 0;
        double ry = 0;
        double rw = 0;
        double rh = 0;
        if (!parseRegionPercents(spec, &rx, &ry, &rw, &rh)) {
            continue;
        }
        if (nearlyEqual(xPct, rx) && nearlyEqual(yPct, ry) && nearlyEqual(wPct, rw) &&
            nearlyEqual(hPct, rh)) {
            settings.endGroup();
            return i;
        }
    }
    settings.endGroup();
    return 0;
}

void DrawRegionController::scheduleSnapAfterClose(qreal xPct, qreal yPct, qreal wPct, qreal hPct)
{
    Q_EMIT requestClose();

    QTimer::singleShot(200, this, [this, xPct, yPct, wPct, hPct]() {
        const int regionIndex = matchingConfiguredRegion(xPct, yPct, wPct, hPct);
        if (regionIndex > 0) {
            m_hasPendingDrawRegionSnap = false;
            logLine(QStringLiteral("draw region matched region %1 (%2 %3 %4 %5)")
                        .arg(regionIndex)
                        .arg(xPct)
                        .arg(yPct)
                        .arg(wPct)
                        .arg(hPct));
            invokeRegionShortcut(regionIndex);
            return;
        }
        logLine(QStringLiteral("draw region custom snap (%1 %2 %3 %4)").arg(xPct).arg(yPct).arg(wPct).arg(hPct));
        invokeDrawRegionSnapShortcut();
    });
}

void DrawRegionController::snapToPercentRect(qreal xPct, qreal yPct, qreal wPct, qreal hPct)
{
    cancelAutoCloseTimer();
    const QString spec =
        QStringLiteral("%1 %2 %3 %4").arg(xPct, 0, 'f', 8).arg(yPct, 0, 'f', 8).arg(wPct, 0, 'f', 8).arg(hPct, 0, 'f', 8);

    m_pendingDrawRegionSnap.clear();
    m_pendingDrawRegionSnap.insert(QStringLiteral("internalId"), m_targetInternalId);
    m_pendingDrawRegionSnap.insert(QStringLiteral("spec"), spec);
    if (m_tilingBasis.isValid()) {
        m_pendingDrawRegionSnap.insert(QStringLiteral("basisX"), m_tilingBasis.x());
        m_pendingDrawRegionSnap.insert(QStringLiteral("basisY"), m_tilingBasis.y());
        m_pendingDrawRegionSnap.insert(QStringLiteral("basisW"), m_tilingBasis.width());
        m_pendingDrawRegionSnap.insert(QStringLiteral("basisH"), m_tilingBasis.height());
        const int G = m_gridGap;
        const int newW = qMax(1, qRound((wPct / 100.0) * (m_tilingBasis.width() - G) - G));
        const int newH = qMax(1, qRound((hPct / 100.0) * (m_tilingBasis.height() - G) - G));
        const int newX = qRound((xPct / 100.0) * (m_tilingBasis.width() - G) + G) + m_tilingBasis.x();
        const int newY = qRound((yPct / 100.0) * (m_tilingBasis.height() - G) + G) + m_tilingBasis.y();
        m_pendingDrawRegionSnap.insert(QStringLiteral("frameX"), newX);
        m_pendingDrawRegionSnap.insert(QStringLiteral("frameY"), newY);
        m_pendingDrawRegionSnap.insert(QStringLiteral("frameW"), newW);
        m_pendingDrawRegionSnap.insert(QStringLiteral("frameH"), newH);
        m_pendingDrawRegionSnap.insert(QStringLiteral("hasFrame"), true);
    } else {
        m_pendingDrawRegionSnap.insert(QStringLiteral("hasFrame"), false);
    }
    m_hasPendingDrawRegionSnap = true;

    QSettings settings(kwinrcPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Script-org.kde.ktile"));
    settings.setValue(QStringLiteral("drawRegionSnap"), spec);
    settings.remove(QStringLiteral("drawRegionSnapAbsolute"));
    if (!m_targetInternalId.isEmpty()) {
        settings.setValue(QStringLiteral("drawRegionTargetInternalId"), m_targetInternalId);
    } else {
        settings.remove(QStringLiteral("drawRegionTargetInternalId"));
    }
    settings.remove(QStringLiteral("drawRegionTargetWindowId"));
    if (m_tilingBasis.isValid()) {
        const int G = m_gridGap;
        const int newW = qMax(1, qRound((wPct / 100.0) * (m_tilingBasis.width() - G) - G));
        const int newH = qMax(1, qRound((hPct / 100.0) * (m_tilingBasis.height() - G) - G));
        const int newX = qRound((xPct / 100.0) * (m_tilingBasis.width() - G) + G) + m_tilingBasis.x();
        const int newY = qRound((yPct / 100.0) * (m_tilingBasis.height() - G) + G) + m_tilingBasis.y();
        settings.setValue(QStringLiteral("drawRegionFrame"),
                         QStringLiteral("%1 %2 %3 %4").arg(newX).arg(newY).arg(newW).arg(newH));
    } else {
        settings.remove(QStringLiteral("drawRegionFrame"));
    }
    settings.endGroup();
    settings.sync();

    logLine(QStringLiteral("snapToPercentRect: %1 target=%2 log=%3")
                .arg(spec, m_targetInternalId, logFilePath()));

    scheduleSnapAfterClose(xPct, yPct, wPct, hPct);
}
