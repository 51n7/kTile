# Packaging kTile

kTile has two installable parts, both installed by **one CMake build**:

1. **KWin script** → `share/kwin/scripts/org.kde.ktile/` (enable under *Window Management → KWin Scripts*).
2. **KCM** → Qt plugin `libkcm_ktile.so` under `lib(64)/qt6/plugins/kf6/kcm/` (shows under *Window Management → kTile*).

## Build dependencies

```bash
sudo dnf install cmake gcc-c++ extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kcmutils-devel kf6-kconfig-devel kf6-kcoreaddons-devel kf6-ki18n-devel
```

Other distributions: install the **KF6** counterparts (KCMUtils, KConfig, KCoreAddons, KI18n), **Qt 6** Base + Declarative dev packages, **CMake**, and **ECM** (extra-cmake-modules).

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

## Install from source (single prefix)

```bash
./install-kcm.sh              # ~/.local
./install-kcm.sh /usr/local   # system-wide if you have write access
```

## Fedora RPM (local `rpmbuild`)

1. Create a tarball from the repository (adjust path):

   ```bash
   cd /path/to/kTile
   git archive --format=tar.gz --prefix=ktile-0.1.0/ -o ~/rpmbuild/SOURCES/ktile-0.1.0.tar.gz HEAD
   ```

2. Copy `packaging/fedora/ktile.spec` to `~/rpmbuild/SPECS/ktile.spec` and fix `Source0` if you host tarballs elsewhere (the spec’s `URL` points at https://github.com/51n7/kTile).
   If you do not have a remote repo yet, keep `Source0` as a local tarball name and build from that.

3. Build:

   ```bash
   rpmbuild -ba ~/rpmbuild/SPECS/ktile.spec
   ```

4. Install the RPM:

   ```bash
   sudo dnf install ~/rpmbuild/RPMS/*/ktile-*.rpm
   ```

After installation, open **System Settings**, enable **kTile** under **KWin Scripts**, and configure regions under **Window Management → kTile**.

## COPR / Flathub

- **COPR**: point a COPR recipe at this repo and use the same spec (or a `rpkg`/`tito` workflow). Users then run `dnf copr enable …` and `dnf install ktile`.
- **Flatpak**: not a good fit here: the KCM and KWin script must load inside the **session** KWin and **system** System Settings; Flatpak sandboxing breaks that integration. Prefer distro packages or `./install-kcm.sh` into `/usr` or `~/.local`.

## Version bumps

Update `project(kTile VERSION …)` in the top-level `CMakeLists.txt`, `Version` in `kcm/kcm_ktile.json`, `Version` in `kwin-script/metadata.json`, and `Version` / `Release` in `packaging/fedora/ktile.spec`.
