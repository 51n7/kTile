#!/usr/bin/env bash
# Remove duplicate/stale kTile entries from KGlobalAccel and kglobalshortcutsrc.
# Run once after upgrade, then reinstall/reload the KWin script and Apply in the kTile KCM.
set -euo pipefail

CFG="${HOME}/.config/kglobalshortcutsrc"
NAMES=(
    "kTile: Open settings"
    "kTile: Move window to next screen"
    "kTile: Open region picker"
    "kTile: Close region picker"
)
for i in $(seq 1 32); do
    NAMES+=("kTile: region ${i}")
done

if [[ -f "$CFG" ]] && grep -q '^kTile:' "$CFG" 2>/dev/null; then
    sed -i '/^kTile:/d' "$CFG"
    echo "removed kTile: keys from $CFG"
else
    echo "no kTile: keys in $CFG"
fi

if busctl --user list org.kde.kglobalaccel &>/dev/null; then
    for name in "${NAMES[@]}"; do
        busctl --user call org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel unregister \
            ss kwin "$name" &>/dev/null || true
    done
    busctl --user call org.kde.kglobalaccel /component/kwin org.kde.kglobalaccel.Component cleanUp &>/dev/null || true
    echo "cleared kTile actions from KGlobalAccel (runtime)"
else
    echo "org.kde.kglobalaccel not available (not in a Plasma session?)"
fi

if busctl --user list org.kde.KWin &>/dev/null; then
    busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting unloadScript s org.kde.ktile &>/dev/null || true
    sleep 0.2
    busctl --user call org.kde.KWin /Scripting org.kde.kwin.Scripting start &>/dev/null || true
    echo "reloaded KWin script org.kde.ktile"
fi

echo "Done. Open System Settings → Window Management → kTile and click Apply if shortcuts are missing."
