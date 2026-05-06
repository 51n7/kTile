/*
   kTile — KWin script
   Snaps the active window to a rectangle (x y width height) from script config.
   Default shortcut: read from kwinrc (key shortcut1); you can also change the
   binding under System Settings → Shortcuts (search for “kTile”).
*/
"use strict";

/**
 * @param {string} spec Whitespace-separated x y width height in global/workspace pixels
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
    const w = Number(area.width);
    const h = Number(area.height);
    if (![x, y, w, h].every(function (n) { return isFinite(n); }) || w <= 0 || h <= 0) {
        return null;
    }
    return { x: x, y: y, w: w, h: h };
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
        try {
            const area = normalizeArea(workspace.clientArea(candidates[i], wnd));
            if (area) {
                return area;
            }
        } catch (e) {
            // Try next API variant/area type.
        }
    }
    return null;
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

function snapActiveToRectFromConfig(configKey, defaultSpec) {
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
    let target = { x: r.x, y: r.y, w: r.w, h: r.h };
    const workArea = readWorkAreaForWindow(wnd);
    if (workArea) {
        const clipped = intersectRect(target, workArea);
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
    // Some windows ignore frameGeometry while maximized/fullscreen.
    if (wnd.fullScreen) {
        wnd.fullScreen = false;
    }
    if (typeof wnd.setMaximize === "function") {
        wnd.setMaximize(false, false);
    }

    // Avoid relying on Qt.rect availability in scripting context.
    wnd.frameGeometry = {
        x: withGap.x,
        y: withGap.y,
        width: withGap.w,
        height: withGap.h
    };
}

function defaultRegionForIndex(oneBasedIndex) {
    return "100 100 960 540";
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
