# Packaging kTile

kTile has two installable parts, both installed by **one CMake build**:

1. **KWin script** → `share/kwin/scripts/org.kde.ktile/` (enable under *Window Management → KWin Scripts*).
2. **KCM** → Qt plugin `libkcm_ktile.so` under `lib(64)/qt6/plugins/kf6/kcm/` (shows under *Window Management → kTile*).

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

## Troubleshooting (KCM / QML)

If System Settings still shows an **old QML error** (for example mentioning `patterns`) after installing a **new `.deb` or RPM**, Plasma is usually loading a **different `kcm_ktile.so`** than the one from the package:

1. **User-prefix install** (`./install-kcm.sh` without `/usr`): `install-kcm.sh` adds `~/.config/plasma-workspace/env/ktile-paths.sh`, which puts **`~/.local` ahead of `/usr` on `QT_PLUGIN_PATH`**. Remove the user-built plugin or run `./uninstall-kcm.sh`, then **log out and back in** (or reboot) so session env reloads.
2. Find copies: `find ~/.local /usr/lib -name kcm_ktile.so 2>/dev/null`
3. Test without extra env: `env -u QT_PLUGIN_PATH kcmshell6 kcm_ktile` (may still inherit Plasma’s defaults — logging out after removing `~/.local` copies is the reliable fix).

## Version bumps

**Upstream version** (the `0.1.0` part):

- Update `project(kTile VERSION ...)` in top-level `CMakeLists.txt`
- `Version` in `kcm/kcm_ktile.json`
- `Version` in `kwin-script/metadata.json`
- `Version:` in `packaging/fedora/ktile.spec`

**Packaging iteration** (the `-8` in `0.1.0-8.fc43` and `0.1.0-8` on Debian): bump **`packaging/PACKAGING_RELEASE`** (single integer, shared by RPM and DEB). Then sync **`packaging/debian/changelog`** (first line `ktile (<upstream>-<iteration>) …`) — use `dch` or edit by hand — and keep **`%global packrel`** in `packaging/fedora/ktile.spec` the same as `PACKAGING_RELEASE` (or rely on `./build.sh`, which substitutes `packrel` from `PACKAGING_RELEASE` when invoking `rpmbuild`).