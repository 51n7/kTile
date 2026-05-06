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
static constexpr int kSettingsJsonVersion = 1;

int jsonVersionFromRoot(const QJsonObject &root)
{
    const QJsonValue v = root.value(QLatin1String("version"));
    if (v.isDouble()) {
        return int(v.toDouble());
    }
    return v.toInt();
}

QString normalizeShortcutSequence(const QString &sequence)
{
    QString s = sequence.trimmed();
    if (s.isEmpty()) {
        return s;
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
            for (int i = 1; i <= regionCount && ok; ++i) {
                if (!names.contains(QStringLiteral("kTile: region %1").arg(i))) {
                    ok = false;
                }
            }
            if (ok) {
                return true;
            }
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(50);
    }
    qCWarning(KCM_KTILE) << "Timed out waiting for kTile actions from KWin (regionCount" << regionCount << ").";
    return false;
}

QString activeShortcutFromSerializedGlobalEntry(const QString &entry)
{
    if (entry.isEmpty()) {
        return QString();
    }
    // kglobalshortcutsrc line: Active,Default,Description — commas also appear *inside* Active
    // (e.g. Meta+Ctrl+, for the comma key), so splitting on every ',' breaks and yields
    // "Meta+Ctrl+\\" while Keyboard settings still shows Meta+Ctrl+,.
    QString rest = entry.trimmed();
    static const QRegularExpression ktileDescSuffix(QStringLiteral(R"(,kTile: region \d+\s*$)"));
    rest.remove(ktileDescSuffix);
    static const QRegularExpression ktileOpenSuffix(QStringLiteral(R"(,kTile: Open settings\s*$)"));
    rest.remove(ktileOpenSuffix);

    const int lastComma = rest.lastIndexOf(QLatin1Char(','));
    QString active = (lastComma > 0) ? rest.left(lastComma) : rest;
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
}

QString KcmKTile::defaultRegionForIndex(int oneBasedIndex)
{
    Q_UNUSED(oneBasedIndex)
    return QStringLiteral("100 100 960 540");
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
    const RegionEntry &first = m_regions.at(0);
    setRepresentsDefaults(first.region == defaultRegionForIndex(1) && first.shortcut == defaultShortcutForIndex(1));
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
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        return screen->virtualGeometry();
    }
    return QRect(0, 0, 1920, 1080);
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
    m_regions.push_back({defaultRegionForIndex(oneBased), defaultShortcutForIndex(oneBased)});
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

QString KcmKTile::exportSettingsJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("kTile"));
    root.insert(QStringLiteral("version"), kSettingsJsonVersion);
    root.insert(QStringLiteral("gridColumns"), m_gridColumns);
    root.insert(QStringLiteral("gridRows"), m_gridRows);
    root.insert(QStringLiteral("gridGap"), m_gridGap);
    root.insert(QStringLiteral("openSettingsShortcut"), normalizeShortcutSequence(m_openSettingsShortcut));

    QJsonArray regionsArr;
    for (const RegionEntry &e : m_regions) {
        QJsonObject o;
        o.insert(QStringLiteral("region"), e.region);
        o.insert(QStringLiteral("shortcut"), normalizeShortcutSequence(e.shortcut));
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
        newRegions.push_back(entry);
    }

    m_gridColumns = std::clamp(
        root.value(QLatin1String("gridColumns")).toInt(kDefaultGridColumns), kGridSpanMin, kGridSpanMax);
    m_gridRows =
        std::clamp(root.value(QLatin1String("gridRows")).toInt(kDefaultGridRows), kGridSpanMin, kGridSpanMax);
    m_gridGap = std::clamp(root.value(QLatin1String("gridGap")).toInt(kDefaultGridGap), 0, kGridGapMax);
    m_openSettingsShortcut = normalizeShortcutSequence(root.value(QLatin1String("openSettingsShortcut")).toString());
    m_regions = std::move(newRegions);

    emitRegionsChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT openSettingsShortcutChanged();
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

    const QString openActionName = QStringLiteral("kTile: Open settings");
    QString openShortcut = normalizeShortcutSequence(g.readEntry(QStringLiteral("openSettingsShortcut"), QString()));
    if (kwinShortcuts.hasKey(openActionName)) {
        const QString serialized = kwinShortcuts.readEntry(openActionName, QString());
        openShortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
    }
    m_openSettingsShortcut = openShortcut;

    m_regions.clear();
    m_regions.reserve(count);
    for (int i = 1; i <= count; ++i) {
        RegionEntry entry;
        entry.region = g.readEntry(QStringLiteral("region%1").arg(i), defaultRegionForIndex(i));
        const QString actionName = QStringLiteral("kTile: region %1").arg(i);
        QString shortcut = normalizeShortcutSequence(
            g.readEntry(QStringLiteral("shortcut%1").arg(i), defaultShortcutForIndex(i)));
        if (kwinShortcuts.hasKey(actionName)) {
            const QString serialized = kwinShortcuts.readEntry(actionName, QString());
            shortcut = normalizeShortcutSequence(activeShortcutFromSerializedGlobalEntry(serialized));
        }
        entry.shortcut = shortcut;
        m_regions.push_back(entry);
    }
    emitRegionsChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT virtualGeometryChanged();
    Q_EMIT openSettingsShortcutChanged();
    updateRepresentsDefaults();
    setNeedsSave(false);
}

void KcmKTile::save()
{
    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(kKWinConfigFile);
    KConfigGroup g(cfg, kScriptConfigGroup);
    g.writeEntry(QStringLiteral("regionCount"), m_regions.size());
    g.writeEntry(QStringLiteral("gridColumns"), m_gridColumns);
    g.writeEntry(QStringLiteral("gridRows"), m_gridRows);
    g.writeEntry(QStringLiteral("gridGap"), m_gridGap);
    g.writeEntry(QStringLiteral("openSettingsShortcut"), normalizeShortcutSequence(m_openSettingsShortcut));
    for (int i = 0; i < m_regions.size(); ++i) {
        const int oneBased = i + 1;
        g.writeEntry(QStringLiteral("region%1").arg(oneBased), m_regions.at(i).region);
        g.writeEntry(QStringLiteral("shortcut%1").arg(oneBased), normalizeShortcutSequence(m_regions.at(i).shortcut));
    }
    for (int i = m_regions.size() + 1; i <= kMaxRegions; ++i) {
        g.deleteEntry(QStringLiteral("region%1").arg(i));
        g.deleteEntry(QStringLiteral("shortcut%1").arg(i));
    }
    cfg->sync();

    // Clear persisted kTile shortcut entries; KWin script re-registration will
    // re-create them from Script-org.kde.ktile/shortcutN defaults.
    KSharedConfig::Ptr shortcutsCfg = KSharedConfig::openConfig(kGlobalShortcutsFile);
    KConfigGroup kwinShortcuts(shortcutsCfg, QStringLiteral("kwin"));
    for (int i = 1; i <= kMaxRegions; ++i) {
        kwinShortcuts.deleteEntry(QStringLiteral("kTile: region %1").arg(i));
    }
    kwinShortcuts.deleteEntry(QStringLiteral("kTile: Open settings"));
    shortcutsCfg->sync();

    // Force runtime re-registration for all kTile region actions so updates to
    // existing shortcuts are applied immediately (not only on first creation).
    QDBusInterface kglobalaccel(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QDBusConnection::sessionBus());
    if (kglobalaccel.isValid()) {
        // Ask KGlobalAccel for real IDs to avoid tuple mismatch issues.
        QDBusReply<QList<QStringList>> actionsReply =
            kglobalaccel.call(QStringLiteral("allActionsForComponent"), QStringList{QStringLiteral("kwin")});
        if (actionsReply.isValid()) {
            static const QRegularExpression regionActionRe(QStringLiteral("^kTile: region (\\d+)$"));
            for (const QStringList &actionId : actionsReply.value()) {
                if (actionId.size() < 2) {
                    continue;
                }
                const QString actionName = actionId.at(1);
                const QRegularExpressionMatch m = regionActionRe.match(actionName);
                if (!m.hasMatch() && actionName != QLatin1String("kTile: Open settings")) {
                    continue;
                }
                kglobalaccel.call(QStringLiteral("unRegister"), actionId);
            }
        }
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

    QDBusInterface kwinComponent(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QDBusConnection::sessionBus());
    if (kwinComponent.isValid()) {
        kwinComponent.call(QStringLiteral("cleanUp"));
    }

    // Apply shortcuts after script registration (and cleanUp) so bindings persist in KGlobalAccel.
    if (kglobalaccel.isValid()) {
        QDBusReply<QList<QStringList>> actionsReply =
            kglobalaccel.call(QStringLiteral("allActionsForComponent"), QStringList{QStringLiteral("kwin")});
        if (actionsReply.isValid()) {
            static const QRegularExpression regionActionRe(QStringLiteral("^kTile: region (\\d+)$"));
            for (const QStringList &actionId : actionsReply.value()) {
                if (actionId.size() < 2) {
                    continue;
                }
                const QString actionName = actionId.at(1);
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
                }
            }
        }
    }

    setNeedsSave(false);
}

void KcmKTile::defaults()
{
    m_gridColumns = kDefaultGridColumns;
    m_gridRows = kDefaultGridRows;
    m_gridGap = kDefaultGridGap;
    m_openSettingsShortcut.clear();
    m_regions.clear();
    m_regions.push_back({defaultRegionForIndex(1), defaultShortcutForIndex(1)});
    emitRegionsChanged();
    Q_EMIT gridLayoutChanged();
    Q_EMIT openSettingsShortcutChanged();
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
    if (!regionActionRe.match(actionName).hasMatch() && actionName != QLatin1String("kTile: Open settings")) {
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
