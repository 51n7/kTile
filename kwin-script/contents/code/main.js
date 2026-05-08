/*
   kTile — KWin script
   Positions the active window using regions from script config.

   coordinateMode=percent (default): regionN is "x% y% w% h%" (0–100, decimals OK),
   relative to the active window's screen — same idea as legacy kTile (declarative)
   tileWindow(): percentages of Workspace.clientArea(FullScreenArea, …).

   coordinateMode=absolute: regionN is global pixel x y width height (legacy KCM).

   Default shortcut: read from kwinrc (shortcutN); see System Settings → Shortcuts.
*/
"use strict";

/**
 * @param {string} spec Whitespace-separated four numbers
 * @returns {{ok: boolean, x?: number, y?: number, w?: number, h?: number, err?: string}}
 */
function parseRect(spec) {
    const parts = String(spec).trim().split(/\s+/);
    if (parts.length < 4) {
        return { ok: false, err: "need four numbers: x y width height" };
    }
    const x = Number(parts[0]);
    const y = Number(parts[1]);
    const w = Number(parts[2]);
    const h = Number(parts[3]);
    if (![x, y, w, h].every(function (n) { return isFinite(n); })) {
        return { ok: false, err: "invalid number" };
    }
    if (w <= 0 || h <= 0) {
        return { ok: false, err: "width and height must be positive" };
    }
    return { ok: true, x: x, y: y, w: w, h: h };
}

function normalizeArea(area) {
    if (!area) {
        return null;
    }
    const x = Number(area.x);
    const y = Number(area.y);
    const wRaw = area.width != null ? area.width : area.w;
    const hRaw = area.height != null ? area.height : area.h;
    const w = Number(wRaw);
    const h = Number(hRaw);
    if (![x, y, w, h].every(function (n) { return isFinite(n); }) || w <= 0 || h <= 0) {
        return null;
    }
    return { x: x, y: y, w: w, h: h };
}

/**
 * Pick the output where the largest portion of the window's frameGeometry lies.
 * On two monitors with the same height, wnd.output / screenAt(center) can still
 * resolve to the wrong (narrower) head, so vertical halves look fine but
 * horizontal % width uses the wrong basis.
 */
function outputDominantByFrameOverlap(wnd) {
    try {
        const fg = wnd.frameGeometry;
        const screens = workspace.screens;
        if (!fg || !screens) {
            return null;
        }
        const wf = normalizeArea({ x: fg.x, y: fg.y, width: fg.width, height: fg.height });
        if (!wf) {
            return null;
        }
        let best = null;
        let bestArea = -1;
        let i = 0;
        for (;;) {
            let o = null;
            try {
                if (typeof screens.length === "number") {
                    if (i >= screens.length) {
                        break;
                    }
                    o = screens[i];
                } else {
                    o = screens[i];
                    if (!o) {
                        break;
                    }
                }
            } catch (e) {
                break;
            }
            if (o) {
                const g = outputGeometryNormalized(o);
                if (g) {
                    const inter = intersectRect(wf, g);
                    const a = inter ? inter.w * inter.h : 0;
                    if (a > bestArea) {
                        bestArea = a;
                        best = o;
                    }
                }
            }
            i += 1;
            if (i > 64) {
                break;
            }
        }
        if (best && bestArea > 0) {
            return best;
        }
    } catch (e) {
        // Fall through.
    }
    return null;
}

/** Output (monitor) the window is on; matches legacy tileWindow activeScreen intent. */
function outputForWindow(wnd) {
    if (!wnd) {
        return null;
    }
    const byOverlap = outputDominantByFrameOverlap(wnd);
    if (byOverlap) {
        return byOverlap;
    }
    try {
        if (wnd.output) {
            return wnd.output;
        }
    } catch (e) {
        // Fall through.
    }
    try {
        const fg = wnd.frameGeometry;
        if (fg && typeof workspace.screenAt === "function") {
            const cx = Number(fg.x) + Number(fg.width) / 2;
            const cy = Number(fg.y) + Number(fg.height) / 2;
            let at = null;
            try {
                at = workspace.screenAt(cx, cy);
            } catch (e2) {
                try {
                    at = workspace.screenAt({ x: cx, y: cy });
                } catch (e3) {
                    at = null;
                }
            }
            if (at) {
                return at;
            }
        }
    } catch (e) {
        // Fall through.
    }
    try {
        const fg = wnd.frameGeometry;
        const screens = workspace.screens;
        if (fg && screens && typeof screens.length === "number" && screens.length) {
            const cx = Number(fg.x) + Number(fg.width) / 2;
            const cy = Number(fg.y) + Number(fg.height) / 2;
            for (let i = 0; i < screens.length; ++i) {
                const o = screens[i];
                const g = outputGeometryNormalized(o);
                if (g && cx >= g.x && cx < g.x + g.w && cy >= g.y && cy < g.y + g.h) {
                    return o;
                }
            }
        }
    } catch (e) {
        // Fall through.
    }
    try {
        if (workspace.activeScreen) {
            return workspace.activeScreen;
        }
    } catch (e) {
        // Fall through.
    }
    return null;
}

function outputGeometryNormalized(out) {
    if (!out) {
        return null;
    }
    try {
        return normalizeArea(out.geometry);
    } catch (e) {
        return null;
    }
}

/** KDE-preferred overload: clientArea(option, output, currentDesktop). */
function readClientAreaOnOutput(option, out, desktop) {
    try {
        if (!out || !desktop) {
            return null;
        }
        return normalizeArea(workspace.clientArea(option, out, desktop));
    } catch (e) {
        return null;
    }
}

/**
 * Prefer workspace.clientArea(option, output, desktop). Fall back to
 * clientArea(option, window) only if needed (can be wrong on some setups).
 */
function readClientAreaForWindow(option, wnd) {
    const out = outputForWindow(wnd);
    let desktop = null;
    try {
        desktop = workspace.currentDesktop;
    } catch (e) {
        desktop = null;
    }
    let a = readClientAreaOnOutput(option, out, desktop);
    if (a) {
        return a;
    }
    try {
        return normalizeArea(workspace.clientArea(option, wnd));
    } catch (e) {
        return null;
    }
}

/** clientArea(option, window) only — tied to the window's monitor in global coords. */
function readClientAreaDirectForWindow(option, wnd) {
    try {
        if (!wnd) {
            return null;
        }
        return normalizeArea(workspace.clientArea(option, wnd));
    } catch (e) {
        return null;
    }
}

function kwinScreensLength() {
    try {
        const screens = workspace.screens;
        if (!screens) {
            return 0;
        }
        if (typeof screens.length === "number") {
            return screens.length;
        }
        let n = 0;
        while (n < 64 && screens[n]) {
            n += 1;
        }
        return n;
    } catch (e) {
        return 0;
    }
}

function outputAtKWinIndex(displayIndex) {
    const di = Number(displayIndex);
    if (!isFinite(di) || di < 0) {
        return null;
    }
    try {
        const screens = workspace.screens;
        if (!screens) {
            return null;
        }
        if (typeof screens.length === "number") {
            if (di >= screens.length) {
                return null;
            }
            return screens[di];
        }
        return screens[di] || null;
    } catch (e) {
        return null;
    }
}

/** Same as legacy QML: move window to head N before applying region (multi-monitor). */
function sendWindowToConfiguredDisplay(wnd, displayIndex) {
    const di = Number(displayIndex);
    if (!isFinite(di) || di < 0) {
        return;
    }
    const out = outputAtKWinIndex(di);
    if (!out) {
        return;
    }
    try {
        if (typeof workspace.sendClientToScreen === "function") {
            workspace.sendClientToScreen(wnd, out);
        }
    } catch (e) {
        // Fall through.
    }
}

function regionOneBasedFromConfigKey(configKey) {
    const m = /^region(\d+)$/.exec(String(configKey || ""));
    if (!m) {
        return 0;
    }
    return parseInt(m[1], 10);
}

function displayIndexForRegionOneBased(oneBased) {
    const raw = readConfig("display" + oneBased, -1);
    const v = Number(raw);
    if (!isFinite(v)) {
        return -1;
    }
    const i = Math.floor(v);
    if (i < 0) {
        return -1;
    }
    if (i >= kwinScreensLength()) {
        return -1;
    }
    return i;
}

/**
 * 100% reference for percent tiling: usable client area intersected with the
 * physical output. clientArea(..., output, desktop) can return a width spanning
 * multiple monitors on some configs — then 50% matches one full monitor.
 * clientArea(option, window) can return a narrow strip — widen via MaximizeArea.
 *
 * We evaluate several area kinds and pick the largest usable rectangle (by area)
 * after capping to the output geometry. Using only the first candidate (e.g.
 * MaximizeArea) can yield a strip or a bad intersection on some multi-monitor /
 * Wayland setups, which makes “50% width” far too narrow.
 */
function percentBasisRectForWindow(wnd) {
    const out = outputForWindow(wnd);
    const geom = outputGeometryNormalized(out);
    let desktop = null;
    try {
        desktop = workspace.currentDesktop;
    } catch (e) {
        desktop = null;
    }
    const order = [];
    if (typeof KWin !== "undefined") {
        order.push(KWin.FullScreenArea);
        order.push(KWin.MaximizeArea);
        order.push(KWin.PlacementArea);
        order.push(KWin.WorkArea);
    }
    let best = null;
    let bestArea = 0;
    for (let i = 0; i < order.length; ++i) {
        const opt = order[i];
        let a = readClientAreaOnOutput(opt, out, desktop);
        if (!a) {
            a = readClientAreaForWindow(opt, wnd);
        }
        if (!a) {
            continue;
        }
        if (geom) {
            const capped = intersectRect(a, geom);
            if (capped && capped.w >= 32 && capped.h >= 32) {
                a = capped;
            }
        }
        if (geom && a.w < geom.w * 0.33 && a.h > geom.h * 0.65) {
            let m = readClientAreaOnOutput(KWin.MaximizeArea, out, desktop);
            if (!m) {
                m = readClientAreaForWindow(KWin.MaximizeArea, wnd);
            }
            if (m && geom) {
                const c2 = intersectRect(m, geom);
                if (c2 && c2.w > a.w && c2.w >= geom.w * 0.4) {
                    a = c2;
                }
            }
        }
        const area = a.w * a.h;
        if (area > bestArea) {
            bestArea = area;
            best = a;
        }
    }
    if (best) {
        return best;
    }
    return geom;
}

function readWorkAreaForWindow(wnd) {
    // Prefer an area that excludes panels/taskbars.
    const candidates = [];
    if (typeof KWin !== "undefined") {
        candidates.push(KWin.MaximizeArea);
        candidates.push(KWin.PlacementArea);
        candidates.push(KWin.WorkArea);
        candidates.push(KWin.FullScreenArea);
    }
    for (let i = 0; i < candidates.length; ++i) {
        const area = readClientAreaForWindow(candidates[i], wnd);
        if (area) {
            return area;
        }
    }
    return null;
}

/** Basis rect for legacy-style percent tiling (see percentBasisRectForWindow). */
function readLegacyFullScreenAreaForWindow(wnd) {
    const b = percentBasisRectForWindow(wnd);
    if (b) {
        return b;
    }
    return readWorkAreaForWindow(wnd);
}

/**
 * Percent grid: 0–100% of a head's FullScreenArea in global coordinates.
 * If displayIndex >= 0: that head (legacy kTile DB display column + sendClientToScreen).
 * If displayIndex < 0 (Auto): clientArea(option, window).
 */
function readPercentTilingBasisRect(wnd, displayIndex) {
    let desktop = null;
    try {
        desktop = workspace.currentDesktop;
    } catch (e) {
        desktop = null;
    }
    const di = Number(displayIndex);
    if (isFinite(di) && di >= 0) {
        const out = outputAtKWinIndex(di);
        if (out && typeof KWin !== "undefined") {
            const order = [
                KWin.MaximizeArea,
                KWin.PlacementArea,
                KWin.WorkArea,
                KWin.FullScreenArea
            ];
            for (let i = 0; i < order.length; ++i) {
                const a = readClientAreaOnOutput(order[i], out, desktop);
                if (a && a.w >= 64 && a.h >= 64) {
                    return a;
                }
            }
        }
    }
    if (typeof KWin !== "undefined") {
        const order = [
            KWin.MaximizeArea,
            KWin.PlacementArea,
            KWin.WorkArea,
            KWin.FullScreenArea
        ];
        for (let j = 0; j < order.length; ++j) {
            const a = readClientAreaDirectForWindow(order[j], wnd);
            if (a && a.w >= 64 && a.h >= 64) {
                return a;
            }
        }
    }
    const geom = outputGeometryNormalized(outputForWindow(wnd));
    if (geom && geom.w >= 64 && geom.h >= 64) {
        return geom;
    }
    return readLegacyFullScreenAreaForWindow(wnd);
}

function readWorkAreaForSnap(wnd, displayIndex) {
    const di = Number(displayIndex);
    let desktop = null;
    try {
        desktop = workspace.currentDesktop;
    } catch (e) {
        desktop = null;
    }
    if (isFinite(di) && di >= 0) {
        const out = outputAtKWinIndex(di);
        if (out && typeof KWin !== "undefined") {
            const order = [KWin.MaximizeArea, KWin.PlacementArea, KWin.WorkArea, KWin.FullScreenArea];
            for (let i = 0; i < order.length; ++i) {
                const a = readClientAreaOnOutput(order[i], out, desktop);
                if (a) {
                    return a;
                }
            }
        }
    }
    return readWorkAreaForWindow(wnd);
}

/** Per-region: legacy pixel rects use values > 100; KCM "percent" mode stays ≤ 100. */
function snapModeForConfig(configKey, defaultSpec) {
    const force = String(readConfig("coordinateMode", "") || "").trim().toLowerCase();
    const spec = readConfig(configKey, defaultSpec);
    const r = parseRect(spec);
    const looksAbsolute = r.ok && (r.x > 100 || r.y > 100 || r.w > 100 || r.h > 100);
    const looksPercent = r.ok && !looksAbsolute;

    if (force === "absolute") {
        // Compatibility: old configs may still force absolute while regionN now stores
        // grid percentages from the KCM. Keep absolute only when the region itself
        // clearly looks like pixels.
        if (looksPercent) {
            return "percent";
        }
        return "absolute";
    }
    if (force === "percent") {
        // Compatibility: if a saved region clearly uses pixels, prefer absolute
        // even when a global percent hint is present.
        if (looksAbsolute) {
            return "absolute";
        }
        return "percent";
    }
    if (!r.ok) {
        return "percent";
    }
    if (looksAbsolute) {
        return "absolute";
    }
    return "percent";
}

function intersectRect(a, b) {
    const x0 = Math.max(a.x, b.x);
    const y0 = Math.max(a.y, b.y);
    const x1 = Math.min(a.x + a.w, b.x + b.w);
    const y1 = Math.min(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        return null;
    }
    return { x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
}

/**
 * Intersect target with the active window's work area. If there is no overlap
 * (common with multi-monitor: region pixels are entirely on another screen),
 * shift and optionally shrink the rectangle so it fits inside the work area while
 * keeping the same intent on *this* display (top-left region → top-left of current
 * screen, etc.).
 */
function intersectRectOrClampToWorkArea(target, wa) {
    if (!wa) {
        return target;
    }
    let clipped = intersectRect(target, wa);
    if (clipped) {
        return clipped;
    }
    const w = Math.min(target.w, wa.w);
    const h = Math.min(target.h, wa.h);
    if (w < 1 || h < 1) {
        return null;
    }
    let x = target.x;
    let y = target.y;
    x = Math.max(wa.x, Math.min(x, wa.x + wa.w - w));
    y = Math.max(wa.y, Math.min(y, wa.y + wa.h - h));
    clipped = intersectRect({ x: x, y: y, w: w, h: h }, wa);
    return clipped;
}

/**
 * Apply gridGap: full gap at work-area edges, half gap on interior edges so two
 * adjacent regions share one full gap; matches margin-to-screen to gap-between-windows.
 */
function applyGapToClippedRect(rect, wa, gap) {
    const G = Math.max(0, Number(gap) || 0);
    if (G <= 0) {
        return rect;
    }
    if (!wa) {
        const half = G / 2;
        const x = rect.x + half;
        const y = rect.y + half;
        const w = rect.w - G;
        const h = rect.h - G;
        if (w < 1 || h < 1) {
            return null;
        }
        return { x: x, y: y, w: w, h: h };
    }
    const eps = 2;
    const leftTouch = rect.x <= wa.x + eps;
    const topTouch = rect.y <= wa.y + eps;
    const rightTouch = rect.x + rect.w >= wa.x + wa.w - eps;
    const bottomTouch = rect.y + rect.h >= wa.y + wa.h - eps;
    const li = leftTouch ? G : G / 2;
    const ti = topTouch ? G : G / 2;
    const ri = rightTouch ? G : G / 2;
    const bi = bottomTouch ? G : G / 2;
    const x = rect.x + li;
    const y = rect.y + ti;
    const w = rect.w - li - ri;
    const h = rect.h - ti - bi;
    if (w < 1 || h < 1) {
        return null;
    }
    return { x: x, y: y, w: w, h: h };
}

function applyFrameGeometry(wnd, x, y, width, height) {
    const xi = Math.round(Number(x));
    const yi = Math.round(Number(y));
    const wi = Math.round(Number(width));
    const hi = Math.round(Number(height));
    if (![xi, yi, wi, hi].every(function (n) { return isFinite(n); })) {
        print("kTile: invalid target frame geometry; skipping");
        return;
    }
    if (wi < 1 || hi < 1) {
        print("kTile: non-positive target frame geometry; skipping", wi, hi);
        return;
    }
    try {
        const current = wnd.frameGeometry;
        if (current &&
            current.x === xi &&
            current.y === yi &&
            current.width === wi &&
            current.height === hi) {
            return;
        }
    } catch (e) {
        // Fall through and try to apply.
    }
    if (wnd.fullScreen) {
        wnd.fullScreen = false;
    }
    if (typeof wnd.setMaximize === "function") {
        wnd.setMaximize(false, false);
    }
    wnd.frameGeometry = {
        x: xi,
        y: yi,
        width: wi,
        height: hi
    };
}

/**
 * Percentages (0–100) of the active window's FullScreenArea, using the same
 * math as legacy ui/main.qml tileWindow() with gap.
 */
function snapActivePercentFromConfig(configKey, defaultSpec) {
    const wnd = workspace.activeWindow;
    if (!wnd) {
        return;
    }
    if (!wnd.normalWindow) {
        return;
    }
    const spec = readConfig(configKey, defaultSpec);
    const r = parseRect(spec);
    if (!r.ok) {
        print("kTile:", r.err, "raw:", spec);
        return;
    }
    const ri = regionOneBasedFromConfigKey(configKey);
    const displayIdx = ri > 0 ? displayIndexForRegionOneBased(ri) : -1;
    sendWindowToConfiguredDisplay(wnd, displayIdx);
    const xPct = r.x;
    const yPct = r.y;
    const wPct = r.w;
    const hPct = r.h;
    const screen = readPercentTilingBasisRect(wnd, displayIdx);
    if (!screen) {
        print("kTile: no screen area for window;", configKey);
        return;
    }
    const G = Math.max(0, Number(readConfig("gridGap", 0)) || 0);
    const newW = (wPct / 100) * (screen.w - G) - G;
    const newH = (hPct / 100) * (screen.h - G) - G;
    const newX = (xPct / 100) * (screen.w - G) + G + screen.x;
    const newY = (yPct / 100) * (screen.h - G) + G + screen.y;
    if (newW < 1 || newH < 1) {
        print("kTile: computed size too small;", configKey, newW, newH);
        return;
    }
    applyFrameGeometry(wnd, newX, newY, newW, newH);
}

/** Legacy global-pixel regions (clip + optional clamp to work area). */
function snapActiveAbsoluteFromConfig(configKey, defaultSpec) {
    const wnd = workspace.activeWindow;
    if (!wnd) {
        return;
    }
    if (!wnd.normalWindow) {
        return;
    }
    const spec = readConfig(configKey, defaultSpec);
    const r = parseRect(spec);
    if (!r.ok) {
        print("kTile:", r.err, "raw:", spec);
        return;
    }
    const ri = regionOneBasedFromConfigKey(configKey);
    const displayIdx = ri > 0 ? displayIndexForRegionOneBased(ri) : -1;
    sendWindowToConfiguredDisplay(wnd, displayIdx);
    let target = { x: r.x, y: r.y, w: r.w, h: r.h };
    const workArea = readWorkAreaForSnap(wnd, displayIdx);
    if (workArea) {
        const clipped = intersectRectOrClampToWorkArea(target, workArea);
        if (!clipped) {
            print("kTile: region outside work area; skipping", configKey);
            return;
        }
        target = clipped;
    }
    const configuredGap = Math.max(0, Number(readConfig("gridGap", 0)) || 0);
    const withGap = applyGapToClippedRect(target, workArea, configuredGap);
    if (!withGap) {
        print("kTile: region too small after applying gap", configKey, configuredGap);
        return;
    }
    applyFrameGeometry(wnd, withGap.x, withGap.y, withGap.w, withGap.h);
}

function snapActiveToRectFromConfig(configKey, defaultSpec) {
    if (snapModeForConfig(configKey, defaultSpec) === "percent") {
        snapActivePercentFromConfig(configKey, defaultSpec);
    } else {
        snapActiveAbsoluteFromConfig(configKey, defaultSpec);
    }
}

function defaultRegionForIndex(oneBasedIndex) {
    void oneBasedIndex;
    return "0 0 50 50";
}

function defaultShortcutForIndex(oneBasedIndex) {
    // Keep empty by default; user binds it in System Settings -> Shortcuts.
    // We still register the action so it appears in the shortcuts KCM.
    void oneBasedIndex;
    return "";
}

function configuredRegionCount() {
    const countValue = Number(readConfig("regionCount", 0));
    if (isFinite(countValue) && countValue > 0) {
        return Math.min(Math.floor(countValue), 32);
    }
    // Backward compatibility with old config that only had region1/shortcut1.
    return 1;
}

function normalizeShortcut(sequence) {
    let s = String(sequence || "").trim();
    if (s === "") {
        return s;
    }
    // Canonicalize aliases to "/" form.
    s = s.replace(/\+Slash/gi, "+/");
    s = s.replace(/\+Question/gi, "+?");
    if (/^slash$/i.test(s)) {
        s = "/";
    }
    if (/^question$/i.test(s)) {
        s = "?";
    }
    return s;
}

for (let i = 1; i <= configuredRegionCount(); ++i) {
    const index = i;
    const configured = readConfig("shortcut" + index, defaultShortcutForIndex(index));
    const shortcut = normalizeShortcut(configured);
    const registered = registerShortcut(
        "kTile: region " + index,
        "kTile: region " + index,
        shortcut,
        function () {
            snapActiveToRectFromConfig("region" + index, defaultRegionForIndex(index));
        }
    );
    if (!registered) {
        print("kTile: failed to register shortcut for region", index, "value:", shortcut);
    }
}

function openKTileSettingsViaDBus() {
    callDBus(
        "org.kde.ktile",
        "/KTile",
        "org.kde.ktile.KTile",
        "open"
    );
}

function moveActiveWindowToNextScreen() {
    const wnd = workspace.activeWindow;
    if (!wnd) {
        return;
    }
    if (!wnd.normalWindow) {
        return;
    }
    try {
        if (typeof workspace.slotWindowToNextScreen === "function") {
            workspace.slotWindowToNextScreen();
        }
    } catch (e) {
        print("kTile: slotWindowToNextScreen failed:", e);
    }
}

{
    const configured = readConfig("openSettingsShortcut", "");
    const shortcut = normalizeShortcut(configured);
    const ok = registerShortcut(
        "kTile: Open settings",
        "kTile: Open settings",
        shortcut,
        function () {
            openKTileSettingsViaDBus();
        }
    );
    if (!ok) {
        print("kTile: failed to register Open settings shortcut, value:", shortcut);
    }
}

{
    const configured = readConfig("moveToNextScreenShortcut", "");
    const shortcut = normalizeShortcut(configured);
    const ok = registerShortcut(
        "kTile: Move window to next screen",
        "kTile: Move window to next screen",
        shortcut,
        moveActiveWindowToNextScreen
    );
    if (!ok) {
        print("kTile: failed to register Move window to next screen shortcut, value:", shortcut);
    }
}
