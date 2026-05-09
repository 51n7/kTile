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

## Version bumps

Modify the version number in:

- Update `project(kTile VERSION ...)` in top-level `CMakeLists.txt`
- `Version` in `kcm/kcm_ktile.json`
- `Version` in `kwin-script/metadata.json`
- `Version` / `Release` in `packaging/fedora/ktile.spec`