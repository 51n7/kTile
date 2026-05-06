#!/usr/bin/env bash
# Remove kTile files installed by CMake (use the same prefix as ./install-kcm.sh).
#
# Usage:
#   ./uninstall-kcm.sh                     # prefix ~/.local
#   ./uninstall-kcm.sh /usr/local          # system prefix (needs write access)
#   ./uninstall-kcm.sh --build-dir BUILD   # delete files listed in that build’s install_manifest.txt
#
# With --build-dir, each path in install_manifest.txt is removed (run cmake --install from that
# build at least once so the manifest exists). The KWin script directory is removed entirely.
#
# Without --build-dir, typical paths under PREFIX are removed (lib and lib64 plugin layouts).
#
# Also removes user-prefix helpers added by install-kcm.sh (unless --no-user-extras):
#   ~/.config/plasma-workspace/env/ktile-paths.sh
#   ~/.config/autostart/ktile-session-helper.desktop
#
# Does not remove kTile keys from ~/.config/kwinrc or kglobalshortcutsrc — remove those in
# System Settings or by hand if you want a full reset.
set -euo pipefail

PREFIX="${HOME}/.local"
BUILD_DIR=""
USER_EXTRAS=1

usage() {
    echo "Usage: $0 [PREFIX]"
    echo "       $0 --build-dir CMAKE_BUILD_DIR [PREFIX]"
    echo "Options:"
    echo "  --build-dir DIR   Use install_manifest.txt from a CMake build directory."
    echo "  --no-user-extras  Do not remove ~/.config/... ktile-paths.sh and autostart desktop."
    echo "Default PREFIX is ${HOME}/.local"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            BUILD_DIR="${2:-}"
            [[ -z "$BUILD_DIR" ]] && { echo "error: --build-dir needs a path." >&2; exit 1; }
            shift 2
            ;;
        --no-user-extras)
            USER_EXTRAS=0
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            PREFIX="$1"
            shift
            ;;
    esac
done

PREFIX="${PREFIX%/}"

remove_user_extras() {
    if [[ "$USER_EXTRAS" -ne 1 ]]; then
        return 0
    fi
    local env_file="${HOME}/.config/plasma-workspace/env/ktile-paths.sh"
    local auto="${HOME}/.config/autostart/ktile-session-helper.desktop"
    if [[ -f "$env_file" ]]; then
        rm -f "$env_file"
        echo "removed $env_file"
    fi
    if [[ -f "$auto" ]]; then
        rm -f "$auto"
        echo "removed $auto"
    fi
}

infer_prefix_from_manifest_line() {
    local f="$1"
    if [[ "$f" == *"/share/"* ]]; then
        echo "${f%%/share/*}"
    elif [[ "$f" == *"/bin/"* ]]; then
        echo "${f%%/bin/*}"
    elif [[ "$f" == *"/lib64/"* ]]; then
        echo "${f%%/lib64/*}"
    elif [[ "$f" == *"/lib/"* ]]; then
        echo "${f%%/lib/*}"
    else
        echo ""
    fi
}

uninstall_from_manifest() {
    local manifest="$BUILD_DIR/install_manifest.txt"
    if [[ ! -f "$manifest" ]]; then
        echo "error: missing $manifest — build kTile and run: cmake --install BUILD_DIR --prefix PREFIX" >&2
        exit 1
    fi

    local inferred
    inferred=$(infer_prefix_from_manifest_line "$(head -1 "$manifest")")
    if [[ -n "$inferred" ]]; then
        PREFIX="$inferred"
    fi
    echo "Using prefix from manifest: $PREFIX"

    local line
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ -z "${line// }" ]] && continue
        if [[ -f "$line" ]]; then
            rm -f "$line"
            echo "removed $line"
        fi
    done <"$manifest"

    # Remove entire KWin script package (manifest only lists some files).
    local dir=""
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ "$line" == *"/share/kwin/scripts/org.kde.ktile/"* ]] || continue
        dir="$line"
        while [[ "$dir" != / && "$(basename "$dir")" != "org.kde.ktile" ]]; do
            dir=$(dirname "$dir")
        done
        if [[ "$(basename "$dir")" == "org.kde.ktile" ]]; then
            rm -rf "$dir"
            echo "removed $dir"
            break
        fi
    done <"$manifest"
}

uninstall_known_prefix_paths() {
    local p="$PREFIX"
    echo "Removing kTile files under prefix: $p"

    rm -f "$p/bin/ktile-session-helper"
    rm -f "$p/share/applications/kcm_ktile.desktop"
    rm -f "$p/etc/xdg/autostart/ktile-session-helper.desktop"

    local lib
    for lib in lib lib64; do
        rm -f "$p/$lib/plugins/plasma/kcms/systemsettings/kcm_ktile.so"
    done

    rm -rf "$p/share/kwin/scripts/org.kde.ktile"

    echo "done."
}

if [[ -n "$BUILD_DIR" ]]; then
    BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
    uninstall_from_manifest
else
    uninstall_known_prefix_paths
fi

remove_user_extras

echo
echo "Optional: stop the session helper if running:  pkill -f ktile-session-helper"
echo "Log out and back in (or restart Plasma) so caches update."
