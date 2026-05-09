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

```bash
sudo apt install cmake build-essential extra-cmake-modules \
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
- **DEB** — runs `fakeroot dpkg-buildpackage -b -us -uc` from the repo root. Warns if `debian/changelog` does not start with `ktile (<version>-…)`.
- **Arch** — builds in a temp dir with `packaging/arch/PKGBUILD`, refreshes `pkgver` and `sha256sums`, then runs `makepkg -f`. Resulting `*.pkg.tar.zst` are left in that temp directory (the script prints the path).

Override the RPM tree if needed:

```bash
RPMBUILD_TOP=/path/to/rpmbuild ./build.sh
```

## Version bumps

Modify the version number in:

- Update `project(kTile VERSION ...)` in top-level `CMakeLists.txt`
- `Version` in `kcm/kcm_ktile.json`
- `Version` in `kwin-script/metadata.json`
- `Version` / `Release` in `packaging/fedora/ktile.spec`