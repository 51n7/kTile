// SPDX-FileCopyrightText: None
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kcmktile.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QScreen>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QCoreApplication>
#include <QSet>
#include <QThread>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

K_PLUGIN_CLASS_WITH_JSON(KcmKTile, "kcm_ktile.json")

namespace
{
Q_LOGGING_CATEGORY(KCM_KTILE, "kcm.ktile", QtWarningMsg)

static constexpr int kFallbackMaxDisplayIndex = 3; // Display 1..4 in KWin → 0..3

static int clampedDisplayIndex(int raw)
{
    const int n = QGuiApplication::screens().size();
    const int maxIdx = n > 0 ? qMax(0, n - 1) : kFallbackMaxDisplayIndex;
    return std::clamp(raw, -1, maxIdx);
}

static const QString kKWinConfigFile{QStringLiteral("kwinrc")};
static const QString kGlobalShortcutsFile{QStringLiteral("kglobalshortcutsrc")};
// Must match KWin::AbstractScript::config() in:
//   return kwinApp()->config()->group("Script-" + m_pluginName);
// with m_pluginName == KPlugin Id of the KWin/Script (metadata.json)
static const QString kScriptConfigGroup{QStringLiteral("Script-org.kde.ktile")};
static constexpr int kMaxRegions = 32;
static constexpr int kDefaultRegionCount = 1;
static constexpr int kGridSpanMin = 1;
static constexpr int kGridSpanMax = 64;
static constexpr int kGridGapMax = 48;
static constexpr int kDefaultGridColumns = 8;
static constexpr int kDefaultGridRows = 6;
static constexpr int kDefaultGridGap = 0;
static constexpr qreal kDefaultRegionPickerOverlayOpacity = 0.30;
static constexpr qreal kDefaultDrawRegionOverlayOpacity = 0.30;
static constexpr bool kDefaultDrawRegionShowGridLines = false;
static constexpr int kDefaultRegionPickerAutoCloseSeconds = 10;
static constexpr int kDefaultDrawRegionAutoCloseSeconds = 5;
static constexpr int kOverlayAutoCloseSecondsMax = 300;

int clampOverlayAutoCloseSeconds(int value)
{
    return std::clamp(value, 0, kOverlayAutoCloseSecondsMax);
}
static constexpr bool kDefaultRegionPickerShowHeader = true;
static constexpr int kSettingsJsonVersion = 1;

qreal clampRegionPickerOverlayOpacity(qreal value)
{
    return std::clamp(value, 0.0, 1.0);
}

int jsonVersionFromRoot(const QJsonObject &root)
{
    const QJsonValue v = root.value(QLatin1String("version"));
    if (v.isDouble()) {
        return int(v.toDouble());
    }
    return v.toInt();
}

QString activeShortcutFromSerializedGlobalEntry(const QString &entry);

QString normalizeShortcutSequence(const QString &sequence)
{
    QString s = sequence.trimmed();
    if (s.isEmpty()) {
        return s;
    }
    // Repair kglobalshortcutsrc-shaped values accidentally stored in kwinrc (e.g. "none,none").
    if (!s.contains(QLatin1String("kTile:")) && s.contains(QLatin1String(",none"))) {
        s = activeShortcutFromSerializedGlobalEntry(s + QLatin1String(",kTile: _repair"));
    }
    if (s.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
        return QString();
    }
    // Malformed parse residue when description suffix was not stripped (older builds).
    if (s == QLatin1String(", ,none") || s == QLatin1String(", , none")) {
        return QString();
    }
    // Collapse duplicate trailing comma key (e.g. "Meta+Ctrl+,," → "Meta+Ctrl+,").
    while (s.endsWith(QLatin1String(",,")) && s.contains(QLatin1Char('+'))) {
        s.chop(1);
    }
    // Keep this in sync with kcm/ui/main.qml normalizeShortcutText() and
    // kwin-script/main.js normalizeShortcut(). Canonicalize to "/" form.
    s.replace(QStringLiteral("+Slash"), QStringLiteral("+/"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("+Question"), QStringLiteral("+?"), Qt::CaseInsensitive);
    if (s.compare(QStringLiteral("Slash"), Qt::CaseInsensitive) == 0) {
        s = QStringLiteral("/");
    }
    if (s.compare(QStringLiteral("Question"), Qt::CaseInsensitive) == 0) {
        s = QStringLiteral("?");
    }
    return s;
}

QList<int> keysListFromShortcutString(const QString &normalized)
{
    QList<int> keys;
    if (normalized.isEmpty()) {
        return keys;
    }

    const auto tryTake = [&](const QKeySequence &seq) -> bool {
        if (seq.isEmpty()) {
            return false;
        }
        const QKeyCombination first = seq[0];
        const int combined = first.toCombined();
        if (combined == 0) {
            return false;
        }
        // Portable text like "Meta+Ctrl+Comma" often parses without modifiers (bogus Key_* only).
        // Real chords such as "Meta+Ctrl+," keep Ctrl/Meta on QKeyCombination.
        if (normalized.contains(QLatin1Char('+')) && first.keyboardModifiers() == Qt::NoModifier) {
            return false;
        }
        keys << combined;
        return true;
    };

    const QKeySequence portable = QKeySequence::fromString(normalized, QKeySequence::PortableText);
    if (tryTake(portable)) {
        return keys;
    }
    const QKeySequence native = QKeySequence::fromString(normalized, QKeySequence::NativeText);
    if (tryTake(native)) {
        return keys;
    }
    return keys;
}

bool isKTileKGlobalActionName(const QString &actionName)
{
    static const QRegularExpression regionActionRe(QStringLiteral("^kTile: region (\\d+)$"));
    if (regionActionRe.match(actionName).hasMatch()) {
        return true;
    }
    return actionName == QLatin1String("kTile: Open settings")
        || actionName == QLatin1String("kTile: Move window to next screen")
        || actionName == QLatin1String("kTile: Open region picker")
        || actionName == QLatin1String("kTile: Draw region on screen")
        || actionName == QLatin1String("kTile: Apply drawn region")
        || actionName == QLatin1String("kTile: Close region picker");
}

QList<QStringList> kTileKWinActionIds(QDBusInterface &kglobalaccel)
{
    QList<QStringList> ids;
    if (!kglobalaccel.isValid()) {
        return ids;
    }
    const QDBusReply<QList<QStringList>> actionsReply =
        kglobalaccel.call(QStringLiteral("allActionsForComponent"), QStringList{QStringLiteral("kwin")});
    if (!actionsReply.isValid()) {
        return ids;
    }
    for (const QStringList &actionId : actionsReply.value()) {
        if (actionId.size() >= 2 && isKTileKGlobalActionName(actionId.at(1))) {
            ids.append(actionId);
        }
    }
    return ids;
}

void unregisterKTileActionId(QDBusInterface &kglobalaccel, const QStringList &actionId)
{
    if (!kglobalaccel.isValid() || actionId.size() < 2) {
        return;
    }
    kglobalaccel.call(QStringLiteral("unRegister"), QVariant::fromValue(actionId));
    kglobalaccel.call(QStringLiteral("unregister"), QStringLiteral("kwin"), actionId.at(1));
}

void unregisterAllKTileKWinActions(QDBusInterface &kglobalaccel)
{
    const QList<QStringList> ids = kTileKWinActionIds(kglobalaccel);
    for (const QStringList &actionId : ids) {
        unregisterKTileActionId(kglobalaccel, actionId);
    }
}

int deduplicateKTileKWinActions(QDBusInterface &kglobalaccel)
{
    QHash<QString, QList<QStringList>> byName;
    for (const QStringList &actionId : kTileKWinActionIds(kglobalaccel)) {
        byName[actionId.at(1)].append(actionId);
    }
    int removed = 0;
    for (auto it = byName.cbegin(); it != byName.cend(); ++it) {
        const QList<QStringList> &list = it.value();
        for (int i = 1; i < list.size(); ++i) {
            unregisterKTileActionId(kglobalaccel, list.at(i));
            ++removed;
        }
    }
    return removed;
}

void clearKTileShortcutConfigEntries()
{
    KSharedConfig::Ptr shortcutsCfg = KSharedConfig::openConfig(kGlobalShortcutsFile);
    KConfigGroup kwinShortcuts(shortcutsCfg, QStringLiteral("kwin"));
    const QStringList keys = kwinShortcuts.keyList();
    for (const QString &key : keys) {
        if (key.startsWith(QLatin1String("kTile:"))) {
            kwinShortcuts.deleteEntry(key);
        }
    }
    shortcutsCfg->sync();
}

bool waitForKTileKWinActionsRegistered(QDBusInterface &kglobalaccel, int regionCount, int timeoutMs)
{
    if (!kglobalaccel.isValid() || regionCount < 1) {
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const QDBusReply<QList<QStringList>> actionsReply =
            kglobalaccel.call(QStringLiteral("allActionsForComponent"), QStringList{QStringLiteral("kwin")});
        if (actionsReply.isValid()) {
            QSet<QString> names;
            for (const QStringList &actionId : actionsReply.value()) {
                if (actionId.size() >= 2) {
                    names.insert(actionId.at(1));
                }
            }
            bool ok = names.contains(QLatin1String("kTile: Open settings"));
            if (ok) {
                ok = names.contains(QLatin1String("kTile: Move window to next screen"));
            }
            if (ok) {
                ok = names.contains(QLatin1String("kTile: Open region picker"));
            }
            if (ok) {
                ok = names.contains(QLatin1String("kTile: Draw region on screen"));
            }
            for (int i = 1; i <= regionCount && ok; ++i) {
                if (!names.contains(QStringLiteral("kTile: region %1").arg(i))) {
                    ok = false;
                }
            }
            if (ok) {
                return true;
            }
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
        QThread::msleep(50);
    }
    qCWarning(KCM_KTILE) << "Timed out waiting for kTile actions from KWin (regionCount" << regionCount << ").";
    return false;
}

QString activeFieldFromActiveDefaultPair(const QString &rest)
{
    // kglobalshortcutsrc stores each binding as Active,Default[,Description]. Commas inside
    // Active are escaped as \,. Find the rightmost *unescaped* comma so Active may be empty
    // (leading comma) without mis-parsing ",Default" or ",," as a literal comma shortcut.
    int split = -1;
    for (int i = rest.size() - 1; i >= 0; --i) {
        if (rest.at(i) != QLatin1Char(',')) {
            continue;
        }
        int backslashes = 0;
        for (int j = i - 1; j >= 0 && rest.at(j) == QLatin1Char('\\'); --j) {
            ++backslashes;
        }
        if (backslashes % 2 == 0) {
            split = i;
            break;
        }
    }
    QString active = split >= 0 ? rest.left(split) : rest;
    // KConfig escape: \, in the file becomes a comma inside the portable sequence.
    active.replace(QLatin1String("\\,"), QLatin1String(","));

    active = active.trimmed();
    if (active.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
        return QString();
    }
    const QStringList alternatives = active.split(QLatin1Char('\t'));
    if (!alternatives.isEmpty()) {
        active = alternatives.constFirst().trimmed();
    }
    return active;
}

QString activeShortcutFromSerializedGlobalEntry(const QString &entry)
{
    if (entry.isEmpty()) {
        return QString();
    }
    QString rest = entry.trimmed();
    static const QRegularExpression ktileDescSuffix(QStringLiteral(R"(,kTile: region \d+\s*$)"));
    rest.remove(ktileDescSuffix);
    static const QRegularExpression ktileOpenSuffix(QStringLiteral(R"(,kTile: Open settings\s*$)"));
    rest.remove(ktileOpenSuffix);
    static const QRegularExpression ktileMoveNextSuffix(QStringLiteral(R"(,kTile: Move window to next screen\s*$)"));
    rest.remove(ktileMoveNextSuffix);
    static const QRegularExpression ktileRegionPickerSuffix(QStringLiteral(R"(,kTile: Open region picker\s*$)"));
    rest.remove(ktileRegionPickerSuffix);
    static const QRegularExpression ktileDrawRegionSuffix(QStringLiteral(R"(,kTile: Draw region on screen\s*$)"));
    rest.remove(ktileDrawRegionSuffix);
    static const QRegularExpression ktileApplyDrawnRegionSuffix(QStringLiteral(R"(,kTile: Apply drawn region\s*$)"));
    rest.remove(ktileApplyDrawnRegionSuffix);
    static const QRegularExpression ktileCloseRegionPickerSuffix(QStringLiteral(R"(,kTile: Close region picker\s*$)"));
    rest.remove(ktileCloseRegionPickerSuffix);

    return activeFieldFromActiveDefaultPair(rest);
}
}

QString KcmKTile::defaultRegionForIndex(int oneBasedIndex)
{
    Q_UNUSED(oneBasedIndex)
    // Percentages of the active window's display (see kwin-script coordinate mode).
    return QStringLiteral("0 0 50 50");
}

QString KcmKTile::defaultShortcutForIndex(int oneBasedIndex)
{
    Q_UNUSED(oneBasedIndex)
    return QString();
}

void KcmKTile::emitRegionsChanged()
{
    Q_EMIT regionsChanged();
}

void KcmKTile::updateRepresentsDefaults()
{
    if (m_regions.size() != kDefaultRegionCount) {
        setRepresentsDefaults(false);
        return;
    }
    if (m_gridColumns != kDefaultGridColumns || m_gridRows != kDefaultGridRows || m_gridGap != kDefaultGridGap) {
        setRepresentsDefaults(false);
        return;
    }
    if (!m_openSettingsShortcut.isEmpty()) {
        setRepresentsDefaults(false);
        return;
    }
    if (!m_moveToNextScreenShortcut.isEmpty()) {
        setRepresentsDefaults(false);
        return;
    }
    if (!m_openRegionPickerShortcut.isEmpty()) {
        setRepresentsDefaults(false);
        return;
    }
    if (!m_openDrawRegionShortcut.isEmpty()) {
        setRepresentsDefaults(false);
        return;
    }
    if (!qFuzzyCompare(m_regionPickerOverlayOpacity + 1.0, kDefaultRegionPickerOverlayOpacity + 1.0)) {
        setRepresentsDefaults(false);
        return;
    }
    if (m_regionPickerAutoCloseSeconds != kDefaultRegionPickerAutoCloseSeconds) {
        setRepresentsDefaults(false);
        return;
    }
    if (!qFuzzyCompare(m_drawRegionOverlayOpacity + 1.0, kDefaultDrawRegionOverlayOpacity + 1.0)) {
        setRepresentsDefaults(false);
        return;
    }
    if (m_drawRegionShowGridLines != kDefaultDrawRegionShowGridLines) {
        setRepresentsDefaults(false);
        return;
    }
    if (m_drawRegionAutoCloseSeconds != kDefaultDrawRegionAutoCloseSeconds) {
        setRepresentsDefaults(false);
        return;
    }
    if (m_regionPickerShowHeader != kDefaultRegionPickerShowHeader) {
        setRepresentsDefaults(false);
        return;
    }
    const RegionEntry &first = m_regions.at(0);
    setRepresentsDefaults(first.region == defaultRegionForIndex(1) && first.shortcut == defaultShortcutForIndex(1)
                          && first.display == -1);
}

KcmKTile::KcmKTile(QObject *parent, const KPluginMetaData &data)
    : KQuickConfigModule(parent, data)
{
    setButtons(Help | Default | Apply);
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QStringLiteral("yourShortcutGotChanged"),
        this,
        SLOT(onGlobalShortcutChanged(QStringList,QList<int>)));
    connectScreenGeometryUpdates();
    load();
}

int KcmKTile::gridColumns() const
{
    return m_gridColumns;
}

void KcmKTile::setGridColumns(int value)
{
    const int v = std::clamp(value, kGridSpanMin, kGridSpanMax);
    if (m_gridColumns == v) {
        return;
    }
    m_gridColumns = v;
    Q_EMIT gridLayoutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

int KcmKTile::gridRows() const
{
    return m_gridRows;
}

void KcmKTile::setGridRows(int value)
{
    const int v = std::clamp(value, kGridSpanMin, kGridSpanMax);
    if (m_gridRows == v) {
        return;
    }
    m_gridRows = v;
    Q_EMIT gridLayoutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

int KcmKTile::gridGap() const
{
    return m_gridGap;
}

void KcmKTile::setGridGap(int value)
{
    const int v = std::clamp(value, 0, kGridGapMax);
    if (m_gridGap == v) {
        return;
    }
    m_gridGap = v;
    Q_EMIT gridLayoutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

QRect KcmKTile::virtualGeometry() const
{
    // Use the union of every screen's geometry in virtual-desktop coordinates.
    // primaryScreen()->virtualGeometry() can disagree with this on some Wayland /
    // mixed-DPI setups; the grid must match the same coordinate space KWin uses for
    // frameGeometry (full desktop), especially with a monitor to the left (negative
    // or split origins).
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return QRect(0, 0, 1920, 1080);
    }
    QRect u = screens.constFirst()->geometry();
    for (int i = 1; i < screens.size(); ++i) {
        u = u.united(screens.at(i)->geometry());
    }
    return u;
}

QStringList KcmKTile::screenChoices() const
{
    QStringList out;
    out << QStringLiteral("Auto");
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (!screens.isEmpty()) {
        out.reserve(1 + screens.size());
        for (int i = 0; i < screens.size(); ++i) {
            out << QStringLiteral("Display %1").arg(i + 1);
        }
        return out;
    }
    // System Settings may run with zero QScreen instances (offscreen/embedded).
    // KWin still has outputs; offer numbered entries matching workspace.screens order.
    out.reserve(1 + kFallbackMaxDisplayIndex + 1);
    for (int i = 0; i <= kFallbackMaxDisplayIndex; ++i) {
        out << QStringLiteral("Display %1").arg(i + 1);
    }
    return out;
}

bool KcmKTile::displaySelectorVisible() const
{
    return QGuiApplication::screens().size() > 1;
}

QString KcmKTile::openSettingsShortcut() const
{
    return m_openSettingsShortcut;
}

void KcmKTile::setOpenSettingsShortcut(const QString &value)
{
    const QString normalized = normalizeShortcutSequence(value);
    if (m_openSettingsShortcut == normalized) {
        return;
    }
    m_openSettingsShortcut = normalized;
    Q_EMIT openSettingsShortcutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

QString KcmKTile::moveToNextScreenShortcut() const
{
    return m_moveToNextScreenShortcut;
}

void KcmKTile::setMoveToNextScreenShortcut(const QString &value)
{
    const QString normalized = normalizeShortcutSequence(value);
    if (m_moveToNextScreenShortcut == normalized) {
        return;
    }
    m_moveToNextScreenShortcut = normalized;
    Q_EMIT moveToNextScreenShortcutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

QString KcmKTile::openRegionPickerShortcut() const
{
    return m_openRegionPickerShortcut;
}

void KcmKTile::setOpenRegionPickerShortcut(const QString &value)
{
    const QString normalized = normalizeShortcutSequence(value);
    if (m_openRegionPickerShortcut == normalized) {
        return;
    }
    m_openRegionPickerShortcut = normalized;
    Q_EMIT openRegionPickerShortcutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

QString KcmKTile::openDrawRegionShortcut() const
{
    return m_openDrawRegionShortcut;
}

void KcmKTile::setOpenDrawRegionShortcut(const QString &value)
{
    const QString normalized = normalizeShortcutSequence(value);
    if (m_openDrawRegionShortcut == normalized) {
        return;
    }
    m_openDrawRegionShortcut = normalized;
    Q_EMIT openDrawRegionShortcutChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

qreal KcmKTile::regionPickerOverlayOpacity() const
{
    return m_regionPickerOverlayOpacity;
}

void KcmKTile::setRegionPickerOverlayOpacity(qreal value)
{
    const qreal v = clampRegionPickerOverlayOpacity(value);
    if (qFuzzyCompare(m_regionPickerOverlayOpacity, v)) {
        return;
    }
    m_regionPickerOverlayOpacity = v;
    Q_EMIT regionPickerOverlayOpacityChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

int KcmKTile::regionPickerAutoCloseSeconds() const
{
    return m_regionPickerAutoCloseSeconds;
}

void KcmKTile::setRegionPickerAutoCloseSeconds(int value)
{
    const int v = clampOverlayAutoCloseSeconds(value);
    if (m_regionPickerAutoCloseSeconds == v) {
        return;
    }
    m_regionPickerAutoCloseSeconds = v;
    Q_EMIT regionPickerAutoCloseSecondsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

qreal KcmKTile::drawRegionOverlayOpacity() const
{
    return m_drawRegionOverlayOpacity;
}

void KcmKTile::setDrawRegionOverlayOpacity(qreal value)
{
    const qreal v = clampRegionPickerOverlayOpacity(value);
    if (qFuzzyCompare(m_drawRegionOverlayOpacity, v)) {
        return;
    }
    m_drawRegionOverlayOpacity = v;
    Q_EMIT drawRegionOverlayOpacityChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

bool KcmKTile::drawRegionShowGridLines() const
{
    return m_drawRegionShowGridLines;
}

void KcmKTile::setDrawRegionShowGridLines(bool value)
{
    if (m_drawRegionShowGridLines == value) {
        return;
    }
    m_drawRegionShowGridLines = value;
    Q_EMIT drawRegionShowGridLinesChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

int KcmKTile::drawRegionAutoCloseSeconds() const
{
    return m_drawRegionAutoCloseSeconds;
}

void KcmKTile::setDrawRegionAutoCloseSeconds(int value)
{
    const int v = clampOverlayAutoCloseSeconds(value);
    if (m_drawRegionAutoCloseSeconds == v) {
        return;
    }
    m_drawRegionAutoCloseSeconds = v;
    Q_EMIT drawRegionAutoCloseSecondsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

bool KcmKTile::regionPickerShowHeader() const
{
    return m_regionPickerShowHeader;
}

void KcmKTile::setRegionPickerShowHeader(bool value)
{
    if (m_regionPickerShowHeader == value) {
        return;
    }
    m_regionPickerShowHeader = value;
    Q_EMIT regionPickerShowHeaderChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::connectScreenGeometryUpdates()
{
    const auto wireScreen = [this](QScreen *screen) {
        if (!screen) {
            return;
        }
        connect(screen, &QScreen::geometryChanged, this, &KcmKTile::virtualGeometryChanged);
        connect(screen, &QScreen::availableGeometryChanged, this, &KcmKTile::virtualGeometryChanged);
        connect(screen, &QScreen::virtualGeometryChanged, this, &KcmKTile::virtualGeometryChanged);
    };
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        wireScreen(screen);
    }
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, wireScreen](QScreen *screen) {
        wireScreen(screen);
        Q_EMIT virtualGeometryChanged();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        Q_EMIT virtualGeometryChanged();
    });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this, wireScreen](QScreen *screen) {
        wireScreen(screen);
        Q_EMIT virtualGeometryChanged();
    });
}

QVariantList KcmKTile::regions() const
{
    QVariantList out;
    out.reserve(m_regions.size());
    for (int i = 0; i < m_regions.size(); ++i) {
        const RegionEntry &entry = m_regions.at(i);
        QVariantMap row;
        row.insert(QStringLiteral("index"), i);
        row.insert(QStringLiteral("number"), i + 1);
        row.insert(QStringLiteral("region"), entry.region);
        row.insert(QStringLiteral("shortcut"), entry.shortcut);
        row.insert(QStringLiteral("display"), entry.display);
        out.push_back(row);
    }
    return out;
}

void KcmKTile::addRegion()
{
    if (m_regions.size() >= kMaxRegions) {
        return;
    }
    const int oneBased = m_regions.size() + 1;
    m_regions.push_back({defaultRegionForIndex(oneBased), defaultShortcutForIndex(oneBased), -1});
    emitRegionsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::removeRegion(int index)
{
    if (index < 0 || index >= m_regions.size()) {
        return;
    }
    if (m_regions.size() <= 1) {
        return;
    }
    m_regions.removeAt(index);
    emitRegionsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::moveRegion(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_regions.size()) {
        return;
    }
    if (toIndex < 0 || toIndex >= m_regions.size()) {
        return;
    }
    if (fromIndex == toIndex) {
        return;
    }
    m_regions.move(fromIndex, toIndex);
    emitRegionsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::setRegionValue(int index, const QString &value)
{
    if (index < 0 || index >= m_regions.size()) {
        return;
    }
    if (m_regions[index].region == value) {
        return;
    }
    m_regions[index].region = value;
    emitRegionsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::setShortcutValue(int index, const QString &value)
{
    if (index < 0 || index >= m_regions.size()) {
        return;
    }
    const QString normalized = normalizeShortcutSequence(value);
    if (m_regions[index].shortcut == normalized) {
        return;
    }
    m_regions[index].shortcut = normalized;
    emitRegionsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::setDisplayValue(int index, int value)
{
    if (index < 0 || index >= m_regions.size()) {
        return;
    }
    const int v = clampedDisplayIndex(value);
    if (m_regions[index].display == v) {
        return;
    }
    m_regions[index].display = v;
    emitRegionsChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

QString KcmKTile::exportSettingsJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("kTile"));
    root.insert(QStringLiteral("version"), kSettingsJsonVersion);
    root.insert(QStringLiteral("gridColumns"), m_gridColumns);
    root.insert(QStringLiteral("gridRows"), m_gridRows);
    root.insert(QStringLiteral("gridGap"), m_gridGap);
    root.insert(QStringLiteral("openSettingsShortcut"), normalizeShortcutSequence(m_openSettingsShortcut));
    root.insert(QStringLiteral("moveToNextScreenShortcut"), normalizeShortcutSequence(m_moveToNextScreenShortcut));
    root.insert(QStringLiteral("openRegionPickerShortcut"), normalizeShortcutSequence(m_openRegionPickerShortcut));
    root.insert(QStringLiteral("openDrawRegionShortcut"), normalizeShortcutSequence(m_openDrawRegionShortcut));
    root.insert(QStringLiteral("regionPickerOverlayOpacity"), m_regionPickerOverlayOpacity);
    root.insert(QStringLiteral("regionPickerAutoCloseSeconds"), m_regionPickerAutoCloseSeconds);
    root.insert(QStringLiteral("drawRegionOverlayOpacity"), m_drawRegionOverlayOpacity);
    root.insert(QStringLiteral("drawRegionShowGridLines"), m_drawRegionShowGridLines);
    root.insert(QStringLiteral("drawRegionAutoCloseSeconds"), m_drawRegionAutoCloseSeconds);
    root.insert(QStringLiteral("regionPickerShowHeader"), m_regionPickerShowHeader);

    QJsonArray regionsArr;
    for (const RegionEntry &e : m_regions) {
        QJsonObject o;
        o.insert(QStringLiteral("region"), e.region);
        o.insert(QStringLiteral("shortcut"), normalizeShortcutSequence(e.shortcut));
        o.insert(QStringLiteral("display"), e.display);
        regionsArr.append(o);
    }
    root.insert(QStringLiteral("regions"), regionsArr);

    const QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString KcmKTile::exportSettingsToUrl(const QUrl &url) const
{
    if (!url.isLocalFile()) {
        return QStringLiteral("Only local files are supported.");
    }
    const QString path = url.toLocalFile();
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return QStringLiteral("Could not open file for writing: %1").arg(path);
    }
    const QByteArray data = exportSettingsJson().toUtf8();
    if (f.write(data) != qint64(data.size())) {
        return QStringLiteral("Could not write file.");
    }
    if (!f.commit()) {
        return QStringLiteral("Could not save file.");
    }
    return QString();
}

QString KcmKTile::importSettingsFromJson(const QString &json)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return QStringLiteral("JSON parse error: %1 at offset %2.")
            .arg(parseErr.errorString())
            .arg(parseErr.offset);
    }
    if (!doc.isObject()) {
        return QStringLiteral("JSON root must be an object.");
    }
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("format")).toString() != QLatin1String("kTile")) {
        return QStringLiteral("Unknown format (expected \"format\": \"kTile\").");
    }
    const int version = jsonVersionFromRoot(root);
    if (version != kSettingsJsonVersion) {
        return QStringLiteral("Unsupported settings version %1 (expected %2).")
            .arg(version)
            .arg(kSettingsJsonVersion);
    }
    const QJsonArray regionsArr = root.value(QLatin1String("regions")).toArray();
    if (regionsArr.isEmpty()) {
        return QStringLiteral("Missing or empty \"regions\" array.");
    }
    if (regionsArr.size() > kMaxRegions) {
        return QStringLiteral("Too many regions (maximum %1).").arg(kMaxRegions);
    }

    QVector<RegionEntry> newRegions;
    newRegions.reserve(regionsArr.size());
    for (const QJsonValue &v : regionsArr) {
        if (!v.isObject()) {
            return QStringLiteral("Invalid region entry (expected an object).");
        }
        const QJsonObject o = v.toObject();
        const QString regionStr = o.value(QLatin1String("region")).toString().trimmed();
        if (regionStr.isEmpty()) {
            return QStringLiteral("Each region must have a non-empty \"region\" string.");
        }
        RegionEntry entry;
        entry.region = regionStr;
        entry.shortcut = normalizeShortcutSequence(o.value(QLatin1String("shortcut")).toString());
        entry.display = clampedDisplayIndex(o.value(QLatin1String("display")).toInt(-1));
        newRegions.push_back(entry);
    }

    m_gridColumns = std::clamp(
        root.value(QLatin1String("gridColumns")).toInt(kDefaultGridColumns), kGridSpanMin, kGridSpanMax);
    m_gridRows =
        std::clamp(root.value(QLatin1String("gridRows")).toInt(kDefaultGridRows), kGridSpanMin, kGridSpanMax);
    m_gridGap = std::clamp(root.value(QLatin1String("gridGap")).toInt(kDefaultGridGap), 0, kGridGapMax);
    m_openSettingsShortcut = normalizeShortcutSequence(root.value(QLatin1String("openSettingsShortcut")).toString());
    m_moveToNextScreenShortcut =
        normalizeShortcutSequence(root.value(QLatin1String("moveToNextScreenShortcut")).toString());
    m_openRegionPickerShortcut =
        normalizeShortcutSequence(root.value(QLatin1String("openRegionPickerShortcut")).toString());
    m_openDrawRegionShortcut =
        normalizeShortcutSequence(root.value(QLatin1String("openDrawRegionShortcut")).toString());
    m_regionPickerOverlayOpacity = clampRegionPickerOverlayOpacity(
        qreal(root.value(QLatin1String("regionPickerOverlayOpacity")).toDouble(kDefaultRegionPickerOverlayOpacity)));
    m_regionPickerAutoCloseSeconds = clampOverlayAutoCloseSeconds(
        root.value(QLatin1String("regionPickerAutoCloseSeconds")).toInt(kDefaultRegionPickerAutoCloseSeconds));
    if (root.contains(QLatin1String("drawRegionOverlayOpacity"))) {
        m_drawRegionOverlayOpacity = clampRegionPickerOverlayOpacity(
            qreal(root.value(QLatin1String("drawRegionOverlayOpacity")).toDouble(kDefaultDrawRegionOverlayOpacity)));
    } else {
        m_drawRegionOverlayOpacity = m_regionPickerOverlayOpacity;
    }
    m_drawRegionShowGridLines =
        root.value(QLatin1String("drawRegionShowGridLines")).toBool(kDefaultDrawRegionShowGridLines);
    m_drawRegionAutoCloseSeconds = clampOverlayAutoCloseSeconds(
        root.value(QLatin1String("drawRegionAutoCloseSeconds")).toInt(kDefaultDrawRegionAutoCloseSeconds));
    m_regionPickerShowHeader = root.value(QLatin1String("regionPickerShowHeader")).toBool(kDefaultRegionPickerShowHeader);
    m_regions = std::move(newRegions);

    emitRegionsChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT openSettingsShortcutChanged();
    Q_EMIT moveToNextScreenShortcutChanged();
    Q_EMIT openRegionPickerShortcutChanged();
    Q_EMIT openDrawRegionShortcutChanged();
    Q_EMIT regionPickerOverlayOpacityChanged();
    Q_EMIT regionPickerAutoCloseSecondsChanged();
    Q_EMIT drawRegionOverlayOpacityChanged();
    Q_EMIT drawRegionShowGridLinesChanged();
    Q_EMIT drawRegionAutoCloseSecondsChanged();
    Q_EMIT regionPickerShowHeaderChanged();
    updateRepresentsDefaults();

    save();
    return QString();
}

QString KcmKTile::importSettingsFromUrl(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return QStringLiteral("Only local files are supported.");
    }
    const QString path = url.toLocalFile();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Could not open file for reading: %1").arg(path);
    }
    const QByteArray data = f.readAll();
    return importSettingsFromJson(QString::fromUtf8(data));
}

void KcmKTile::load()
{
    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(kKWinConfigFile);
    KConfigGroup g(cfg, kScriptConfigGroup);
    KSharedConfig::Ptr shortcutsCfg = KSharedConfig::openConfig(kGlobalShortcutsFile);
    KConfigGroup kwinShortcuts(shortcutsCfg, QStringLiteral("kwin"));

    int count = g.readEntry(QStringLiteral("regionCount"), 0);
    if (count <= 0) {
        count = 0;
        for (int i = 1; i <= kMaxRegions; ++i) {
            const QString regionKey = QStringLiteral("region%1").arg(i);
            const QString shortcutKey = QStringLiteral("shortcut%1").arg(i);
            if (!g.hasKey(regionKey) && !g.hasKey(shortcutKey)) {
                break;
            }
            ++count;
        }
    }
    if (count <= 0) {
        count = 1;
    }
    count = std::clamp(count, 1, kMaxRegions);

    m_gridColumns = std::clamp(
        g.readEntry(QStringLiteral("gridColumns"), kDefaultGridColumns),
        kGridSpanMin,
        kGridSpanMax);
    m_gridRows =
        std::clamp(g.readEntry(QStringLiteral("gridRows"), kDefaultGridRows), kGridSpanMin, kGridSpanMax);
    m_gridGap = std::clamp(g.readEntry(QStringLiteral("gridGap"), kDefaultGridGap), 0, kGridGapMax);
    m_regionPickerOverlayOpacity = clampRegionPickerOverlayOpacity(
        g.readEntry(QStringLiteral("regionPickerOverlayOpacity"), kDefaultRegionPickerOverlayOpacity));
    m_regionPickerAutoCloseSeconds = clampOverlayAutoCloseSeconds(
        g.readEntry(QStringLiteral("regionPickerAutoCloseSeconds"), kDefaultRegionPickerAutoCloseSeconds));
    if (g.hasKey(QStringLiteral("drawRegionOverlayOpacity"))) {
        m_drawRegionOverlayOpacity = clampRegionPickerOverlayOpacity(
            g.readEntry(QStringLiteral("drawRegionOverlayOpacity"), kDefaultDrawRegionOverlayOpacity));
    } else {
        // Existing installs used region-picker opacity for draw-region overlay.
        m_drawRegionOverlayOpacity = m_regionPickerOverlayOpacity;
    }
    m_drawRegionShowGridLines =
        g.readEntry(QStringLiteral("drawRegionShowGridLines"), kDefaultDrawRegionShowGridLines);
    m_drawRegionAutoCloseSeconds = clampOverlayAutoCloseSeconds(
        g.readEntry(QStringLiteral("drawRegionAutoCloseSeconds"), kDefaultDrawRegionAutoCloseSeconds));
    m_regionPickerShowHeader =
        g.readEntry(QStringLiteral("regionPickerShowHeader"), kDefaultRegionPickerShowHeader);

    // KWin's script only reads shortcuts from kwinrc (readConfig). Keyboard Shortcuts edits
    // kglobalshortcutsrc. If those diverge, the UI shows the global binding but registerShortcut
    // still uses the stale kwinrc value until Apply — or forever if the user never Apply'd from
    // this KCM. Persist merged shortcuts back to kwinrc and reload the script when we repair.
    bool shortcutsRepaired = false;

    const QString openActionName = QStringLiteral("kTile: Open settings");
    const QString storedOpenShortcut =
        normalizeShortcutSequence(g.readEntry(QStringLiteral("openSettingsShortcut"), QString()));
    QString openShortcut = storedOpenShortcut;
    if (kwinShortcuts.hasKey(openActionName)) {
        const QString serialized = kwinShortcuts.readEntry(openActionName, QString());
        openShortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
    }
    if (openShortcut != storedOpenShortcut) {
        g.writeEntry(QStringLiteral("openSettingsShortcut"), openShortcut);
        shortcutsRepaired = true;
    }
    m_openSettingsShortcut = openShortcut;

    const QString nextScreenActionName = QStringLiteral("kTile: Move window to next screen");
    const QString storedNextShortcut =
        normalizeShortcutSequence(g.readEntry(QStringLiteral("moveToNextScreenShortcut"), QString()));
    QString nextShortcut = storedNextShortcut;
    if (kwinShortcuts.hasKey(nextScreenActionName)) {
        const QString serialized = kwinShortcuts.readEntry(nextScreenActionName, QString());
        nextShortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
    }
    if (nextShortcut != storedNextShortcut) {
        g.writeEntry(QStringLiteral("moveToNextScreenShortcut"), nextShortcut);
        shortcutsRepaired = true;
    }
    m_moveToNextScreenShortcut = nextShortcut;

    const QString regionPickerActionName = QStringLiteral("kTile: Open region picker");
    const QString storedRegionPickerShortcut =
        normalizeShortcutSequence(g.readEntry(QStringLiteral("openRegionPickerShortcut"), QString()));
    QString regionPickerShortcut = storedRegionPickerShortcut;
    if (kwinShortcuts.hasKey(regionPickerActionName)) {
        const QString serialized = kwinShortcuts.readEntry(regionPickerActionName, QString());
        regionPickerShortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
    }
    if (regionPickerShortcut != storedRegionPickerShortcut) {
        g.writeEntry(QStringLiteral("openRegionPickerShortcut"), regionPickerShortcut);
        shortcutsRepaired = true;
    }
    m_openRegionPickerShortcut = regionPickerShortcut;

    const QString drawRegionActionName = QStringLiteral("kTile: Draw region on screen");
    const QString storedDrawRegionShortcut =
        normalizeShortcutSequence(g.readEntry(QStringLiteral("openDrawRegionShortcut"), QString()));
    QString drawRegionShortcut = storedDrawRegionShortcut;
    if (kwinShortcuts.hasKey(drawRegionActionName)) {
        const QString serialized = kwinShortcuts.readEntry(drawRegionActionName, QString());
        drawRegionShortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
    }
    if (drawRegionShortcut != storedDrawRegionShortcut) {
        g.writeEntry(QStringLiteral("openDrawRegionShortcut"), drawRegionShortcut);
        shortcutsRepaired = true;
    }
    m_openDrawRegionShortcut = drawRegionShortcut;

    m_regions.clear();
    m_regions.reserve(count);
    for (int i = 1; i <= count; ++i) {
        RegionEntry entry;
        entry.region = g.readEntry(QStringLiteral("region%1").arg(i), defaultRegionForIndex(i));
        const QString actionName = QStringLiteral("kTile: region %1").arg(i);
        const QString storedShortcut = normalizeShortcutSequence(
            g.readEntry(QStringLiteral("shortcut%1").arg(i), defaultShortcutForIndex(i)));
        QString shortcut = storedShortcut;
        if (kwinShortcuts.hasKey(actionName)) {
            const QString serialized = kwinShortcuts.readEntry(actionName, QString());
            shortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
        }
        if (shortcut != storedShortcut) {
            g.writeEntry(QStringLiteral("shortcut%1").arg(i), shortcut);
            shortcutsRepaired = true;
        }
        entry.shortcut = shortcut;
        entry.display = clampedDisplayIndex(g.readEntry(QStringLiteral("display%1").arg(i), -1));
        m_regions.push_back(entry);
    }

    if (shortcutsRepaired) {
        cfg->sync();
        reconfigureKWin();
        reloadKWinScript();
    }

    QDBusInterface kglobalaccel(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QDBusConnection::sessionBus());
    if (deduplicateKTileKWinActions(kglobalaccel) > 0) {
        qCWarning(KCM_KTILE) << "Removed duplicate kTile entries from KGlobalAccel.";
    }

    emitRegionsChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT virtualGeometryChanged();
    Q_EMIT openSettingsShortcutChanged();
    Q_EMIT moveToNextScreenShortcutChanged();
    Q_EMIT openRegionPickerShortcutChanged();
    Q_EMIT openDrawRegionShortcutChanged();
    Q_EMIT regionPickerOverlayOpacityChanged();
    Q_EMIT regionPickerAutoCloseSecondsChanged();
    Q_EMIT drawRegionOverlayOpacityChanged();
    Q_EMIT drawRegionShowGridLinesChanged();
    Q_EMIT drawRegionAutoCloseSecondsChanged();
    Q_EMIT regionPickerShowHeaderChanged();
    updateRepresentsDefaults();
    setNeedsSave(false);
}

void KcmKTile::save()
{
    if (m_saveInProgress) {
        return;
    }
    m_saveInProgress = true;
    struct SaveGuard {
        KcmKTile *self;
        ~SaveGuard()
        {
            self->m_saveInProgress = false;
        }
    } guard{this};

    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(kKWinConfigFile);
    KConfigGroup g(cfg, kScriptConfigGroup);
    // The current KCM edits regions as percentages of the active display.
    // Persist this explicitly so stale legacy "absolute" mode does not
    // reinterpret grid selections as pixel coordinates.
    g.writeEntry(QStringLiteral("coordinateMode"), QStringLiteral("percent"));
    g.writeEntry(QStringLiteral("regionCount"), m_regions.size());
    g.writeEntry(QStringLiteral("gridColumns"), m_gridColumns);
    g.writeEntry(QStringLiteral("gridRows"), m_gridRows);
    g.writeEntry(QStringLiteral("gridGap"), m_gridGap);
    g.writeEntry(QStringLiteral("openSettingsShortcut"), normalizeShortcutSequence(m_openSettingsShortcut));
    g.writeEntry(QStringLiteral("moveToNextScreenShortcut"), normalizeShortcutSequence(m_moveToNextScreenShortcut));
    g.writeEntry(QStringLiteral("openRegionPickerShortcut"), normalizeShortcutSequence(m_openRegionPickerShortcut));
    g.writeEntry(QStringLiteral("openDrawRegionShortcut"), normalizeShortcutSequence(m_openDrawRegionShortcut));
    g.writeEntry(QStringLiteral("regionPickerOverlayOpacity"), m_regionPickerOverlayOpacity);
    g.writeEntry(QStringLiteral("regionPickerAutoCloseSeconds"), m_regionPickerAutoCloseSeconds);
    g.writeEntry(QStringLiteral("drawRegionOverlayOpacity"), m_drawRegionOverlayOpacity);
    g.writeEntry(QStringLiteral("drawRegionShowGridLines"), m_drawRegionShowGridLines);
    g.writeEntry(QStringLiteral("drawRegionAutoCloseSeconds"), m_drawRegionAutoCloseSeconds);
    g.writeEntry(QStringLiteral("regionPickerShowHeader"), m_regionPickerShowHeader);
    for (int i = 0; i < m_regions.size(); ++i) {
        const int oneBased = i + 1;
        g.writeEntry(QStringLiteral("region%1").arg(oneBased), m_regions.at(i).region);
        g.writeEntry(QStringLiteral("shortcut%1").arg(oneBased), normalizeShortcutSequence(m_regions.at(i).shortcut));
        g.writeEntry(QStringLiteral("display%1").arg(oneBased), m_regions.at(i).display);
    }
    for (int i = m_regions.size() + 1; i <= kMaxRegions; ++i) {
        g.deleteEntry(QStringLiteral("region%1").arg(i));
        g.deleteEntry(QStringLiteral("shortcut%1").arg(i));
        g.deleteEntry(QStringLiteral("display%1").arg(i));
    }
    cfg->sync();

    // Drop stale kTile rows from kglobalshortcutsrc and runtime KGlobalAccel before reload.
    clearKTileShortcutConfigEntries();

    QDBusInterface kglobalaccel(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QDBusConnection::sessionBus());
    unregisterAllKTileKWinActions(kglobalaccel);

    QDBusInterface kwinComponent(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QDBusConnection::sessionBus());
    if (kwinComponent.isValid()) {
        kwinComponent.call(QStringLiteral("cleanUp"));
    }

    reconfigureKWin();
    reloadKWinScript();

    // registerShortcut() runs asynchronously after the script loads; applying shortcuts before the
    // actions exist is a no-op and leaves Keyboard Settings unchanged.
    const bool actionsReady = waitForKTileKWinActionsRegistered(kglobalaccel, m_regions.size(), 8000);
    if (!actionsReady) {
        // Do not proceed with cleanup/rebinding if KWin never exposed the kTile actions
        // (e.g. script is disabled/not loaded). Keep config saved and leave runtime bindings as-is.
        qCWarning(KCM_KTILE) << "kTile actions are unavailable after reload; skipping KGlobalAccel cleanup/rebind.";
        setNeedsSave(false);
        return;
    }

    deduplicateKTileKWinActions(kglobalaccel);

    // Apply shortcuts after script registration so bindings persist in KGlobalAccel.
    if (kglobalaccel.isValid()) {
        QDBusReply<QList<QStringList>> actionsReply =
            kglobalaccel.call(QStringLiteral("allActionsForComponent"), QStringList{QStringLiteral("kwin")});
        if (actionsReply.isValid()) {
            static const QRegularExpression regionActionRe(QStringLiteral("^kTile: region (\\d+)$"));
            QSet<QString> reboundNames;
            for (const QStringList &actionId : actionsReply.value()) {
                if (actionId.size() < 2) {
                    continue;
                }
                const QString actionName = actionId.at(1);
                if (reboundNames.contains(actionName)) {
                    continue;
                }
                reboundNames.insert(actionName);
                const QRegularExpressionMatch m = regionActionRe.match(actionName);
                if (m.hasMatch()) {
                    const int regionNumber = m.captured(1).toInt();
                    if (regionNumber < 1 || regionNumber > m_regions.size()) {
                        continue;
                    }
                    const QString shortcutText = m_regions.at(regionNumber - 1).shortcut;
                    const QList<int> keys = keysListFromShortcutString(shortcutText);
                    if (keys.isEmpty() && !shortcutText.isEmpty()) {
                        qCWarning(KCM_KTILE) << "Could not convert shortcut to key codes for" << actionName
                                             << "text:" << shortcutText;
                    }
                    kglobalaccel.call(
                        QStringLiteral("setForeignShortcut"),
                        QVariant::fromValue(actionId),
                        QVariant::fromValue(keys));
                    continue;
                }
                if (actionName == QLatin1String("kTile: Open settings")) {
                    const QList<int> keys = keysListFromShortcutString(m_openSettingsShortcut);
                    if (keys.isEmpty() && !m_openSettingsShortcut.isEmpty()) {
                        qCWarning(KCM_KTILE) << "Could not convert shortcut to key codes for" << actionName
                                             << "text:" << m_openSettingsShortcut;
                    }
                    kglobalaccel.call(
                        QStringLiteral("setForeignShortcut"),
                        QVariant::fromValue(actionId),
                        QVariant::fromValue(keys));
                    continue;
                }
                if (actionName == QLatin1String("kTile: Move window to next screen")) {
                    const QList<int> keys = keysListFromShortcutString(m_moveToNextScreenShortcut);
                    if (keys.isEmpty() && !m_moveToNextScreenShortcut.isEmpty()) {
                        qCWarning(KCM_KTILE) << "Could not convert shortcut to key codes for" << actionName
                                             << "text:" << m_moveToNextScreenShortcut;
                    }
                    kglobalaccel.call(
                        QStringLiteral("setForeignShortcut"),
                        QVariant::fromValue(actionId),
                        QVariant::fromValue(keys));
                    continue;
                }
                if (actionName == QLatin1String("kTile: Open region picker")) {
                    const QList<int> keys = keysListFromShortcutString(m_openRegionPickerShortcut);
                    if (keys.isEmpty() && !m_openRegionPickerShortcut.isEmpty()) {
                        qCWarning(KCM_KTILE) << "Could not convert shortcut to key codes for" << actionName
                                             << "text:" << m_openRegionPickerShortcut;
                    }
                    kglobalaccel.call(
                        QStringLiteral("setForeignShortcut"),
                        QVariant::fromValue(actionId),
                        QVariant::fromValue(keys));
                    continue;
                }
                if (actionName == QLatin1String("kTile: Draw region on screen")) {
                    const QList<int> keys = keysListFromShortcutString(m_openDrawRegionShortcut);
                    if (keys.isEmpty() && !m_openDrawRegionShortcut.isEmpty()) {
                        qCWarning(KCM_KTILE) << "Could not convert shortcut to key codes for" << actionName
                                             << "text:" << m_openDrawRegionShortcut;
                    }
                    kglobalaccel.call(
                        QStringLiteral("setForeignShortcut"),
                        QVariant::fromValue(actionId),
                        QVariant::fromValue(keys));
                    continue;
                }
            }
        }
    }

    // Do not persist kTile rows in kglobalshortcutsrc: bindings live in kwinrc and are
    // applied when the KWin script registers shortcuts (avoids duplicate actions on login).
    clearKTileShortcutConfigEntries();

    setNeedsSave(false);
}

void KcmKTile::defaults()
{
    m_gridColumns = kDefaultGridColumns;
    m_gridRows = kDefaultGridRows;
    m_gridGap = kDefaultGridGap;
    m_openSettingsShortcut.clear();
    m_moveToNextScreenShortcut.clear();
    m_openRegionPickerShortcut.clear();
    m_openDrawRegionShortcut.clear();
    m_regionPickerOverlayOpacity = kDefaultRegionPickerOverlayOpacity;
    m_regionPickerAutoCloseSeconds = kDefaultRegionPickerAutoCloseSeconds;
    m_drawRegionOverlayOpacity = kDefaultDrawRegionOverlayOpacity;
    m_drawRegionShowGridLines = kDefaultDrawRegionShowGridLines;
    m_drawRegionAutoCloseSeconds = kDefaultDrawRegionAutoCloseSeconds;
    m_regionPickerShowHeader = kDefaultRegionPickerShowHeader;
    m_regions.clear();
    m_regions.push_back({defaultRegionForIndex(1), defaultShortcutForIndex(1), -1});
    emitRegionsChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT openSettingsShortcutChanged();
    Q_EMIT moveToNextScreenShortcutChanged();
    Q_EMIT openRegionPickerShortcutChanged();
    Q_EMIT openDrawRegionShortcutChanged();
    Q_EMIT regionPickerOverlayOpacityChanged();
    Q_EMIT regionPickerAutoCloseSecondsChanged();
    Q_EMIT drawRegionOverlayOpacityChanged();
    Q_EMIT drawRegionShowGridLinesChanged();
    Q_EMIT drawRegionAutoCloseSecondsChanged();
    Q_EMIT regionPickerShowHeaderChanged();
    setNeedsSave(true);
    updateRepresentsDefaults();
}

void KcmKTile::onGlobalShortcutChanged(const QStringList &actionId, const QList<int> &newKeys)
{
    Q_UNUSED(newKeys)
    if (actionId.size() < 2) {
        return;
    }
    const QString actionName = actionId.at(1);
    static const QRegularExpression regionActionRe(QStringLiteral("^kTile: region (\\d+)$"));
    if (!regionActionRe.match(actionName).hasMatch() && actionName != QLatin1String("kTile: Open settings")
        && actionName != QLatin1String("kTile: Move window to next screen")
        && actionName != QLatin1String("kTile: Open region picker")
        && actionName != QLatin1String("kTile: Draw region on screen")) {
        return;
    }
    // Do not overwrite in-progress unsaved edits in this KCM.
    if (needsSave()) {
        return;
    }
    load();
}

void KcmKTile::reloadKWinScript() const
{
    QDBusInterface scripting(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"),
        QDBusConnection::sessionBus());
    if (!scripting.isValid()) {
        return;
    }

    QDBusReply<bool> loadedReply = scripting.call(QStringLiteral("isScriptLoaded"), QStringLiteral("org.kde.ktile"));
    if (loadedReply.isValid() && loadedReply.value()) {
        scripting.call(QStringLiteral("unloadScript"), QStringLiteral("org.kde.ktile"));
        QThread::msleep(150);
    }
    scripting.call(QStringLiteral("start"));
}

void KcmKTile::reconfigureKWin() const
{
    QDBusInterface kwin(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QDBusConnection::sessionBus());
    if (!kwin.isValid()) {
        qCWarning(KCM_KTILE) << "org.kde.KWin not available; restart the session for changes to apply.";
        return;
    }
    kwin.call(QStringLiteral("reconfigure"));
}

#include "kcmktile.moc"
