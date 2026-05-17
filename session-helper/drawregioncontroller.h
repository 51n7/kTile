// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QRect>
#include <QVariantMap>

class DrawRegionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal overlayOpacity READ overlayOpacity NOTIFY overlayOpacityChanged)
    Q_PROPERTY(int gridColumns READ gridColumns NOTIFY gridLayoutChanged)
    Q_PROPERTY(int gridRows READ gridRows NOTIFY gridLayoutChanged)
    Q_PROPERTY(int gridGap READ gridGap NOTIFY gridLayoutChanged)
    Q_PROPERTY(bool showGridLines READ showGridLines NOTIFY showGridLinesChanged)

public:
    explicit DrawRegionController(QObject *parent = nullptr);

    qreal overlayOpacity() const;
    int gridColumns() const;
    int gridRows() const;
    int gridGap() const;
    bool showGridLines() const;
    QRect tilingBasisRect() const;

    Q_INVOKABLE void reloadFromConfig();
    Q_INVOKABLE void closeOverlay();
    Q_INVOKABLE void setTargetWindowInternalId(const QString &internalId);
    Q_INVOKABLE void setDrawRegionTilingBasis(int x, int y, int width, int height);
    Q_INVOKABLE void snapToPercentRect(qreal xPct, qreal yPct, qreal wPct, qreal hPct);
    QVariantMap takePendingDrawRegionSnap();

Q_SIGNALS:
    void overlayOpacityChanged();
    void gridLayoutChanged();
    void showGridLinesChanged();
    void requestClose();

private:
    void invokeDrawRegionSnapShortcut();
    void invokeRegionShortcut(int oneBasedIndex);
    int matchingConfiguredRegion(qreal xPct, qreal yPct, qreal wPct, qreal hPct) const;
    void scheduleSnapAfterClose(qreal xPct, qreal yPct, qreal wPct, qreal hPct);

    qreal m_overlayOpacity = 0.30;
    int m_gridColumns = 8;
    int m_gridRows = 6;
    int m_gridGap = 0;
    bool m_showGridLines = false;
    QString m_targetInternalId;
    QRect m_tilingBasis;
    QVariantMap m_pendingDrawRegionSnap;
    bool m_hasPendingDrawRegionSnap = false;
};
