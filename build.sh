#!/usr/bin/env bash
# Interactive local packaging for kTile (RPM / DEB / Arch).
# Prerequisites per distro: see PACKAGING.md

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

extract_version() {
    local line v
    line=$(grep -m1 '^project(kTile' CMakeLists.txt) || true
    v=$(sed -n 's/.*VERSION \([0-9][0-9.]*\).*/\1/p' <<<"$line")
    if [[ -z "$v" ]]; then
        echo "error: could not parse VERSION from CMakeLists.txt (project(kTile VERSION …))." >&2
        exit 1
    fi
    printf '%s' "$v"
}

VERSION="$(extract_version)"

echo "kTile — local package build"
echo "Detected project version (CMakeLists.txt): ${VERSION}"
echo
echo "  1) RPM   Fedora / RHEL-style (rpmbuild; packaging/fedora/ktile.spec)"
echo "  2) DEB   Debian / Ubuntu (dpkg-buildpackage; packaging/debian)"
echo "  3) Arch   pacman package (makepkg; packaging/arch/PKGBUILD)"
echo "  q) Quit"
echo
read -r -p "Choice [1-3/q]: " choice || true

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: '$1' not found. Install packaging tools for your distro (see PACKAGING.md)." >&2
        exit 1
    fi
}

warn_spec_version() {
    local spec_ver
    spec_ver=$(grep -m1 '^Version:' "${ROOT}/packaging/fedora/ktile.spec" | awk '{print $2}')
    if [[ -n "$spec_ver" && "$spec_ver" != "$VERSION" ]]; then
        echo "warning: CMake VERSION (${VERSION}) != RPM spec Version (${spec_ver}). Bump the spec before release." >&2
    fi
}

build_rpm() {
    need_cmd rpmbuild
    warn_spec_version
    local top="${RPMBUILD_TOP:-$HOME/rpmbuild}"
    mkdir -p "${top}"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
    local spec_src="${ROOT}/packaging/fedora/ktile.spec"
    [[ -f "$spec_src" ]] || {
        echo "error: missing ${spec_src}" >&2
        exit 1
    }
    local tarball="${top}/SOURCES/ktile-${VERSION}.tar.gz"
    echo "Writing source tarball: ${tarball}"
    (
        cd "$ROOT"
        tar -czf "$tarball" \
            --exclude-vcs \
            --exclude='./build-cmake' \
            --exclude='./rpmbuild' \
            --exclude='./.cursor' \
            --transform "s|^\\./|ktile-${VERSION}/|" \
            .
    )
    cp -f "$spec_src" "${top}/SPECS/ktile.spec"
    echo "Running: rpmbuild --define \"_topdir ${top}\" -ba …"
    rpmbuild --define "_topdir ${top}" -ba "${top}/SPECS/ktile.spec"
    echo
    echo "Built RPMs: ${top}/RPMS/"
    echo "Built SRPM: ${top}/SRPMS/"
}

deb_changelog_matches() {
    local cl
    cl=$(head -1 "${ROOT}/packaging/debian/changelog")
    [[ "$cl" =~ ^ktile\ \(${VERSION}-[0-9]+\) ]]
}

build_deb() {
    need_cmd dpkg-buildpackage
    if ! command -v fakeroot >/dev/null 2>&1; then
        echo "error: fakeroot not found (e.g. apt install fakeroot)." >&2
        exit 1
    fi
    # dpkg-buildpackage expects ./debian at the repo root; canonical files live under packaging/debian/.
    # If a plain directory named debian/ already exists, ln would put a link inside it instead of replacing it.
    rm -rf "${ROOT}/debian"
    ln -sfn "${ROOT}/packaging/debian" "${ROOT}/debian"
    if ! deb_changelog_matches; then
        echo "warning: packaging/debian/changelog does not start with ktile (${VERSION}-…) — bump it with dch or edit by hand." >&2
        read -r -p "Continue anyway? [y/N] " a || true
        [[ "${a:-}" =~ ^[yY]$ ]] || exit 1
    fi
    if ! (cd "${ROOT}" && dpkg-checkbuilddeps); then
        cat >&2 <<'EOF'
error: install Debian build dependencies (see PACKAGING.md “Ubuntu/Debian”), for example:
  sudo apt install debhelper fakeroot cmake build-essential extra-cmake-modules \
    qt6-base-dev qt6-declarative-dev \
    libkf6kcmutils-dev libkf6config-dev libkf6coreaddons-dev libkf6i18n-dev
EOF
        exit 1
    fi
    echo "Running: fakeroot dpkg-buildpackage -b -us -uc"
    (cd "${ROOT}" && fakeroot dpkg-buildpackage -b -us -uc)
    echo
    local parent deb_out
    parent="$(cd "${ROOT}/.." && pwd)"
    # dpkg-buildpackage writes outputs next to the repo; move them to ~/debian (override with KTILE_DEB_OUT).
    deb_out="${KTILE_DEB_OUT:-${HOME}/debian}"
    mkdir -p "${deb_out}"
    deb_out="$(cd "${deb_out}" && pwd)"
    (
        shopt -s nullglob
        local artifacts=(
            "${parent}"/ktile*.deb
            "${parent}"/ktile*.buildinfo
            "${parent}"/ktile*.changes
        )
        if [[ ${#artifacts[@]} -eq 0 ]]; then
            echo "warning: no ktile build artifacts in ${parent}/ (build may have failed)." >&2
        elif [[ "${parent}" != "${deb_out}" ]]; then
            local f
            for f in "${artifacts[@]}"; do
                mv -f "${f}" "${deb_out}/"
            done
            echo "Moved Debian build products to: ${deb_out}/"
            shopt -s nullglob
            local listed=(
                "${deb_out}"/ktile*.deb
                "${deb_out}"/ktile*.buildinfo
                "${deb_out}"/ktile*.changes
            )
            printf '  %s\n' "${listed[@]}"
        else
            echo "Debian build products (already under destination):"
            printf '  %s\n' "${artifacts[@]}"
        fi
    )
}

build_arch() {
    need_cmd makepkg
    local work pkgbuild tarball sum
    work="$(mktemp -d "${TMPDIR:-/tmp}/ktile-makepkg.XXXXXX")"
    tarball="${work}/ktile-${VERSION}.tar.gz"
    echo "Creating ${tarball}"
    (
        cd "$ROOT"
        tar -czf "$tarball" \
            --exclude-vcs \
            --exclude='./build-cmake' \
            --exclude='./rpmbuild' \
            --exclude='./.cursor' \
            --transform "s|^\\./|ktile-${VERSION}/|" \
            .
    )
    cp "${ROOT}/packaging/arch/PKGBUILD" "${work}/"
    pkgbuild="${work}/PKGBUILD"
    sed -i "s/^pkgver=.*/pkgver=${VERSION}/" "$pkgbuild"
    sum=$(sha256sum "$tarball" | awk '{print $1}')
    sed -i "s/^sha256sums=.*/sha256sums=('${sum}')/" "$pkgbuild"
    cd "$work"
    echo "Running makepkg in: ${work}"
    makepkg -f
    echo
    echo "Done. Look for *.pkg.tar.zst in:"
    echo "  ${work}"
}

case "${choice:-}" in
    1) build_rpm ;;
    2) build_deb ;;
    3) build_arch ;;
    q | Q) exit 0 ;;
    *)
        echo "No valid choice; exiting." >&2
        exit 1
        ;;
esac
