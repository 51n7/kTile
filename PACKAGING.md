# Packaging kTile

kTile has **three** installable parts, all produced by **one CMake build** (`CMakeLists.txt` adds `kcm/`, `session-helper/`, and installs `kwin-script/`):

| Part | Source | Typical install paths |
|------|--------|------------------------|
| **KWin script** | `kwin-script/` | `share/kwin/scripts/org.kde.ktile/` — enable under *Window Management → KWin Scripts* |
| **KCM** | `kcm/` | `lib/qt6/plugins/plasma/kcms/systemsettings/kcm_ktile.so`, `share/applications/kcm_ktile.desktop` |
| **Session helper** | `session-helper/` | `bin/ktile-session-helper`, `etc/xdg/autostart/ktile-session-helper.desktop`, `share/dbus-1/services/org.kde.ktile.service` |

### Why a session helper?

The **region picker** (fullscreen overlay, Escape to close, click-outside to dismiss) is implemented in `session-helper/` as a normal Qt Quick app that autostarts in the user session. The KWin script stays JavaScript-only: it registers shortcuts and calls D-Bus (`showRegionPicker`, `open`). The KCM is on-demand settings UI and is not running while you work in other apps.

Do **not** bind a global **Escape** shortcut to “kTile: Close region picker” in System Settings — older builds used that approach; it interferes with KRunner. Current builds close the picker via local focus in the overlay.

Picker-related sources: `session-helper/main.cpp` (D-Bus + window host), `regionpickercontroller.{h,cpp}` (read `kwinrc`, snap via KGlobalAccel), `qml/RegionPicker.qml`, `qml/RegionBlock.qml`.

## Install build dependencies

### Fedora Linux (example)

```bash
sudo dnf install cmake gcc-c++ extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kcmutils-devel kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel
```

### Arch Linux (example)

```bash
sudo pacman -S --needed cmake extra-cmake-modules base-devel \
  qt6-base qt6-declarative kcmutils kconfig kcoreaddons ki18n
```

### Ubuntu/Debian (example)

For `./install-kcm.sh` from source:

```bash
sudo apt install cmake build-essential extra-cmake-modules \
  qt6-base-dev qt6-declarative-dev \
  libkf6kcmutils-dev libkf6config-dev libkf6coreaddons-dev libkf6i18n-dev
```

To build a `.deb` with `./build.sh`, also install Debian packaging helpers (`debhelper` satisfies `debhelper-compat`; `fakeroot` is required by the script):

```bash
sudo apt install debhelper fakeroot cmake build-essential extra-cmake-modules \
  qt6-base-dev qt6-declarative-dev \
  libkf6kcmutils-dev libkf6config-dev libkf6coreaddons-dev libkf6i18n-dev
```

Other distributions: install the **KF6** counterparts (KCMUtils, KConfig, KCoreAddons, KI18n), **Qt 6** Base + Declarative dev packages, **CMake**, and **ECM** (extra-cmake-modules).

## Install from source

```bash
./install-kcm.sh              # ~/.local
./install-kcm.sh /usr/local   # system-wide if you have write access
```

## Build Distro Package

Use the interactive helper (same **Build dependencies** as above, plus your distro’s packaging tools: `rpmbuild`, or `dpkg-buildpackage` + `fakeroot`, or `makepkg`):

```bash
./build.sh
```

The script reads the version from `project(kTile VERSION …)` in the top-level `CMakeLists.txt`, then lets you build:

- **RPM** — writes `ktile-<version>.tar.gz` under `~/rpmbuild/SOURCES` (or `$RPMBUILD_TOP` if set), copies `packaging/fedora/ktile.spec`, and runs `rpmbuild -ba`. Warns if the spec’s `Version:` does not match CMake.
- **DEB** — Debian metadata lives in `packaging/debian/`. The build script symlinks `./debian` there and runs `fakeroot dpkg-buildpackage -b -us -uc`. Build products (`.deb`, `.buildinfo`, `.changes`) are first emitted next to the repo, then **moved to `~/debian/`** (same role as `~/rpmbuild` for RPM). Warns if `packaging/debian/changelog` does not start with `ktile (<version>-…)`. Override the destination with `KTILE_DEB_OUT`.
- **Arch** — builds in a temp dir with `packaging/arch/PKGBUILD`, refreshes `pkgver` and `sha256sums`, then runs `makepkg -f`. Resulting `*.pkg.tar.zst` are left in that temp directory (the script prints the path).

Override the RPM tree if needed:

```bash
RPMBUILD_TOP=/path/to/rpmbuild ./build.sh
```

Override where Debian build products are placed after the build (default: `~/debian`):

```bash
KTILE_DEB_OUT=/path/to/out ./build.sh
```

## Install the built package

After **`./build.sh`**, install the binary package on the same machine:

**Fedora / RHEL-style (RPM):**

```bash
./build.sh
sudo dnf install ~/rpmbuild/RPMS/*/ktile-*.rpm
```

**Debian / Ubuntu (`.deb`):**

```bash
./build.sh
sudo dpkg -i ~/debian/ktile-*.deb
```

If **`dpkg`** reports missing dependencies, run **`sudo apt-get install -f`** afterward.

## After installing a package

1. Enable **kTile** under *System Settings → Window Management → KWin Scripts* (or reload the script after upgrades).
2. Confirm **`ktile-session-helper`** is running (`pgrep -a ktile-session-helper`). Packages install an autostart entry; logging out and back in once is enough if shortcuts or the picker do nothing.
3. Configure regions and shortcuts under *Window Management → kTile*. Assign **Open region picker** on the General tab if you want the overlay.

Diagnostics: `~/.cache/ktile-session-helper.log` (DBus and launcher messages).

