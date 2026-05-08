// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <KPluginMetaData>
#include <KQuickConfigModule>
#include <QRect>
#include <QUrl>
#include <QVariantList>
#include <QString>
#include <QVector>

class KcmKTile : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList regions READ regions NOTIFY regionsChanged)
    Q_PROPERTY(int gridColumns READ gridColumns WRITE setGridColumns NOTIFY gridLayoutChanged)
    Q_PROPERTY(int gridRows READ gridRows WRITE setGridRows NOTIFY gridLayoutChanged)
    Q_PROPERTY(int gridGap READ gridGap WRITE setGridGap NOTIFY gridLayoutChanged)
    Q_PROPERTY(QRect virtualGeometry READ virtualGeometry NOTIFY virtualGeometryChanged)
    Q_PROPERTY(QStringList screenChoices READ screenChoices NOTIFY virtualGeometryChanged)
    Q_PROPERTY(bool displaySelectorVisible READ displaySelectorVisible NOTIFY virtualGeometryChanged)
    Q_PROPERTY(QString openSettingsShortcut READ openSettingsShortcut WRITE setOpenSettingsShortcut NOTIFY openSettingsShortcutChanged)
    Q_PROPERTY(QString moveToNextScreenShortcut READ moveToNextScreenShortcut WRITE setMoveToNextScreenShortcut NOTIFY moveToNextScreenShortcutChanged)

public:
    KcmKTile(QObject *parent, const KPluginMetaData &data);

    QVariantList regions() const;
    Q_INVOKABLE void addRegion();
    Q_INVOKABLE void removeRegion(int index);
    Q_INVOKABLE void moveRegion(int fromIndex, int toIndex);
    Q_INVOKABLE void setRegionValue(int index, const QString &value);
    Q_INVOKABLE void setShortcutValue(int index, const QString &value);
    Q_INVOKABLE void setDisplayValue(int index, int value);

    int gridColumns() const;
    void setGridColumns(int value);
    int gridRows() const;
    void setGridRows(int value);
    int gridGap() const;
    void setGridGap(int value);
    QRect virtualGeometry() const;
    QStringList screenChoices() const;
    bool displaySelectorVisible() const;

    QString openSettingsShortcut() const;
    void setOpenSettingsShortcut(const QString &value);

    QString moveToNextScreenShortcut() const;
    void setMoveToNextScreenShortcut(const QString &value);

    Q_INVOKABLE QString exportSettingsJson() const;
    Q_INVOKABLE QString exportSettingsToUrl(const QUrl &url) const;
    Q_INVOKABLE QString importSettingsFromJson(const QString &json);
    Q_INVOKABLE QString importSettingsFromUrl(const QUrl &url);

    void load() override;
    void save() override;
    void defaults() override;

Q_SIGNALS:
    void regionsChanged();
    void gridLayoutChanged();
    void virtualGeometryChanged();
    void openSettingsShortcutChanged();
    void moveToNextScreenShortcutChanged();

private:
    struct RegionEntry {
        QString region;
        QString shortcut;
        int display = -1; // -1 = auto (window's screen); else QGuiApplication::screens() index
    };

private Q_SLOTS:
    void onGlobalShortcutChanged(const QStringList &actionId, const QList<int> &newKeys);

private:
    void reloadKWinScript() const;
    void reconfigureKWin() const;
    void updateRepresentsDefaults();
    void emitRegionsChanged();
    static QString defaultRegionForIndex(int oneBasedIndex);
    static QString defaultShortcutForIndex(int oneBasedIndex);
    void connectScreenGeometryUpdates();

    QVector<RegionEntry> m_regions;
    QString m_openSettingsShortcut;
    QString m_moveToNextScreenShortcut;
    int m_gridColumns = 8;
    int m_gridRows = 6;
    int m_gridGap = 0;
};
