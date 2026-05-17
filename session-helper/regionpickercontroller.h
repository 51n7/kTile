// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

class RegionPickerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList regions READ regions NOTIFY regionsChanged)
    Q_PROPERTY(qreal overlayOpacity READ overlayOpacity NOTIFY overlayOpacityChanged)
    Q_PROPERTY(bool showPickerHeader READ showPickerHeader NOTIFY showPickerHeaderChanged)
    Q_PROPERTY(int autoCloseSeconds READ autoCloseSeconds NOTIFY autoCloseSecondsChanged)

public:
    explicit RegionPickerController(QObject *parent = nullptr);

    QVariantList regions() const;
    qreal overlayOpacity() const;
    bool showPickerHeader() const;
    int autoCloseSeconds() const;

    Q_INVOKABLE void reloadFromConfig();
    Q_INVOKABLE void beginAutoCloseTimer();
    Q_INVOKABLE void cancelAutoCloseTimer();
    Q_INVOKABLE void snapToRegion(int oneBasedIndex);
    Q_INVOKABLE void closePicker();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void openDrawRegion();

    void invokeRegionShortcut(int oneBasedIndex);
    void invokeDrawRegionShortcut();

    /** Clear stale global Escape binding from older kTile builds. */
    static void purgeStaleClosePickerEscape();

Q_SIGNALS:
    void regionsChanged();
    void overlayOpacityChanged();
    void showPickerHeaderChanged();
    void autoCloseSecondsChanged();
    void requestClose();

private:
    struct RegionPreview {
        int index = 0;
        double x = 0;
        double y = 0;
        double w = 0;
        double h = 0;
    };

    QVector<RegionPreview> m_regions;
    qreal m_overlayOpacity = 0.30;
    bool m_showPickerHeader = true;
    int m_autoCloseSeconds = 10;
    QTimer m_autoCloseTimer;
};
