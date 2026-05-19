# SPDX-License-Identifier: GPL-2.0-or-later
# kTile — KWin snap script + System Settings KCM for KDE Plasma 6

%global forgeurl https://github.com/51n7/kTile
# Replace Source0 with your tarball URL or use forge macros after publishing.

Name:           ktile
Version:        0.1.1
# Packaging iteration — canonical value is packaging/PACKAGING_RELEASE (build.sh substitutes when copying to rpmbuild).
%global packrel 2
Release:        %{packrel}%{?dist}
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
kTile ships a KWin script, a System Settings page (KCM), and a session helper
(ktile-session-helper) for the region-picker overlay and org.kde.ktile D-Bus
API. You define screen regions and snap the active window with shortcuts, or
pick a region from the overlay. See README.md and PACKAGING.md in the source tree.

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
%{_datadir}/dbus-1/services/org.kde.ktile.service
%{_datadir}/applications/kcm_ktile.desktop
%{_datadir}/kwin/scripts/org.kde.ktile/
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_ktile.so

%changelog
* Thu May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-8
- session-helper: add runtime log at ~/.cache/ktile-session-helper.log for DBus/open diagnostics

* Thu May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-7
- session-helper: launch KCM with environment inherited from running Plasma/KWin session processes

* Thu May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-6
- session-helper: resolve kcmshell6/systemsettings via absolute paths and fallback launcher

* Thu May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-5
- session-helper: switch to QCoreApplication to avoid GUI/display init failures on DBus activation

* Thu May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-4
- Add DBus activation for org.kde.ktile so Open settings shortcut works without relogin

* Thu May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-3
- KCM: move per-region display selector inline into region header actions row

* Wed May 07 2026 kTile upstream <packaging@ktile.local> - 0.1.0-2
- KCM: show Display row when System Settings has no QScreen*; fallback display list

* Tue May 05 2026 kTile upstream <packaging@ktile.local> - 0.1.0-1
- Initial package
