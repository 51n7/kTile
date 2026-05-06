# SPDX-License-Identifier: GPL-2.0-or-later
# kTile — KWin snap script + System Settings KCM for KDE Plasma 6

%global forgeurl https://github.com/51n7/kTile
# Replace Source0 with your tarball URL or use forge macros after publishing.

Name:           ktile
Version:        0.1.0
Release:        1%{?dist}
Summary:        Custom window snap regions for KDE Plasma (KWin script + KCM)

License:        GPL-2.0-or-later
URL:            %{forgeurl}

# Source tarball: see PACKAGING.md or ./build.sh (git archive prefix must be ktile-VERSION/).
Source0:        ktile-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel

Requires:       kwin
# KCM UI: Kirigami + KCMUtils are pulled in by systemsettings on typical installs;
# add explicit Requires if a minimal spin omits them:
Requires:       plasma-workspace
Requires:       kf6-kcmutils
Requires:       kf6-kirigami
Requires:       kf6-kdeclarative

%description
kTile ships a KWin script and a System Settings page (KCM) under Window
Management. You define a rectangle (x, y, width, height) and snap the active
window there with a shortcut (default Meta+Shift+1). This is separate from
KWin's built-in tiling.

%prep
%autosetup -n ktile-%{version} -p1

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install
# KCM installs to QT_INSTALL_PLUGINS/plasma/kcms/systemsettings when installing
# into /usr with distro Qt (ECM KDE_INSTALL_USE_QT_SYS_PATHS), not plain LIBDIR/plugins.

%files
%license LICENSE
%{_bindir}/ktile-session-helper
%{_sysconfdir}/xdg/autostart/ktile-session-helper.desktop
%{_datadir}/applications/kcm_ktile.desktop
%{_datadir}/kwin/scripts/org.kde.ktile/
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_ktile.so

%changelog
* Tue May 05 2026 kTile upstream <packaging@ktile.local> - 0.1.0-1
- Initial package
