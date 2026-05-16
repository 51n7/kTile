// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVariantList>

class RegionPickerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList regions READ regions NOTIFY regionsChanged)

public:
    explicit RegionPickerController(QObject *parent = nullptr);

    QVariantList regions() const;

    Q_INVOKABLE void reloadFromConfig();
    Q_INVOKABLE void snapToRegion(int oneBasedIndex);
    Q_INVOKABLE void closePicker();
    Q_INVOKABLE void openSettings();

    void invokeRegionShortcut(int oneBasedIndex);

    /** Clear stale global Escape binding from older kTile builds. */
    static void purgeStaleClosePickerEscape();

Q_SIGNALS:
    void regionsChanged();
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
};
