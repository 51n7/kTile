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

extract_packaging_release() {
    local line f
    f="${ROOT}/packaging/PACKAGING_RELEASE"
    if [[ ! -f "$f" ]]; then
        echo "error: missing ${f} — add a single integer (RPM Release / Debian revision)." >&2
        exit 1
    fi
    line=$(grep -v '^[[:space:]]*#' "$f" | grep -v '^[[:space:]]*$' | head -1 || true)
    line=$(printf '%s' "$line" | tr -d ' \t\r')
    if [[ ! "$line" =~ ^[1-9][0-9]*$ ]]; then
        echo "error: packaging/PACKAGING_RELEASE must contain one positive integer (got '${line}')." >&2
        exit 1
    fi
    printf '%s' "$line"
}

# Uses global VERSION and PKGREL (must be set before call).
deb_changelog_matches() {
    local cl
    cl=$(head -1 "${ROOT}/packaging/debian/changelog" 2>/dev/null || true)
    [[ "$cl" =~ ^ktile\ \(${VERSION}-${PKGREL}\) ]]
}

# Prepend a stanza so the first line matches ktile (VERSION-PKGREL) … (DEB only; keeps Debian id aligned with RPM/Arch).
# Requires packaging/debian/ (called from build_deb only — do not require debian metadata for RPM/Arch on Fedora).
sync_debian_changelog() {
    local cl="${ROOT}/packaging/debian/changelog"
    local debci="${ROOT}/packaging/debian/control"
    local tmp datestamp
    if [[ ! -f "$debci" ]]; then
        echo "error: DEB build needs ${debci} (use a full clone with packaging/debian/)." >&2
        exit 1
    fi
    if [[ ! -f "$cl" ]]; then
        datestamp="$(date -R 2>/dev/null || date)"
        {
            echo "ktile (${VERSION}-${PKGREL}) unstable; urgency=medium"
            echo
            echo "  * Package build (${VERSION}-${PKGREL})."
            echo
            echo " -- kTile upstream <packaging@ktile.local>  ${datestamp}"
        } >"$cl"
        echo "Created ${cl}." >&2
        return 0
    fi
    if deb_changelog_matches; then
        return 0
    fi
    tmp="$(mktemp "${TMPDIR:-/tmp}/ktile-changelog.XXXXXX")"
    datestamp="$(date -R 2>/dev/null || date)"
    {
        echo "ktile (${VERSION}-${PKGREL}) unstable; urgency=medium"
        echo
        echo "  * Sync Debian changelog with packaging (upstream ${VERSION}, release ${PKGREL})."
        echo
        echo " -- kTile upstream <packaging@ktile.local>  ${datestamp}"
        echo
        cat "$cl"
    } >"$tmp"
    mv -f "$tmp" "$cl"
    echo "Updated packaging/debian/changelog to start with ktile (${VERSION}-${PKGREL})." >&2
}

bump_patch_version() {
    local v="$1"
    if [[ "$v" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        printf '%s.%s.%s' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "$((BASH_REMATCH[3] + 1))"
        return 0
    fi
    return 1
}

write_packaging_release() {
    printf '%s\n' "$1" >"${ROOT}/packaging/PACKAGING_RELEASE"
}

write_cmake_version() {
    local v="$1"
    local spec="${ROOT}/CMakeLists.txt"
    if ! grep -q '^project(kTile VERSION ' "$spec"; then
        echo "error: could not find project(kTile VERSION …) in CMakeLists.txt" >&2
        exit 1
    fi
    sed -i "s/^project(kTile VERSION [0-9][0-9.]* LANGUAGES CXX)/project(kTile VERSION ${v} LANGUAGES CXX)/" "$spec"
}

write_spec_version() {
    local v="$1"
    local spec="${ROOT}/packaging/fedora/ktile.spec"
    [[ -f "$spec" ]] || {
        echo "error: missing ${spec}" >&2
        exit 1
    }
    sed -i "s/^Version:.*/Version:        ${v}/" "$spec"
}

write_spec_packrel() {
    local n="$1"
    local spec="${ROOT}/packaging/fedora/ktile.spec"
    sed -i "s/^%global packrel .*/%global packrel ${n}/" "$spec"
}

write_pkgbuild_versions() {
    local v="$1" rel="$2"
    local pkgbuild="${ROOT}/packaging/arch/PKGBUILD"
    [[ -f "$pkgbuild" ]] || return 0
    sed -i "s/^pkgver=.*/pkgver=${v}/" "$pkgbuild"
    sed -i "s/^pkgrel=.*/pkgrel=${rel}/" "$pkgbuild"
}

prompt_release_bump() {
    local next_patch suggested
    next_patch="$(bump_patch_version "$VERSION" 2>/dev/null || true)"

    echo
    echo "Release type for this package build:"
    echo "  r) Rebuild same upstream version — bump packaging release (${VERSION}-${PKGREL} -> ${VERSION}-$((PKGREL + 1)))"
    if [[ -n "$next_patch" ]]; then
        echo "  n) New upstream version — bump to ${next_patch} and reset packaging release to 1"
    else
        echo "  n) New upstream version — enter a version and reset packaging release to 1"
    fi
    echo "  s) Skip — keep ${VERSION}-${PKGREL} (no file changes)"
    read -r -p "Choice [r/n/s]: " rel_choice || true

    case "${rel_choice,,}" in
        r)
            PKGREL=$((PKGREL + 1))
            write_packaging_release "$PKGREL"
            write_spec_packrel "$PKGREL"
            write_pkgbuild_versions "$VERSION" "$PKGREL"
            echo "Set packaging release to ${PKGREL} (package id ${VERSION}-${PKGREL})."
            ;;
        n)
            suggested="$next_patch"
            if [[ -z "$suggested" ]]; then
                read -r -p "New upstream version (e.g. 0.2.0): " suggested || true
            else
                read -r -p "New upstream version [${suggested}]: " custom || true
                if [[ -n "${custom:-}" ]]; then
                    suggested="$custom"
                fi
            fi
            if [[ ! "$suggested" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
                echo "error: version must look like MAJOR.MINOR.PATCH (got '${suggested}')." >&2
                exit 1
            fi
            VERSION="$suggested"
            PKGREL=1
            write_cmake_version "$VERSION"
            write_spec_version "$VERSION"
            write_packaging_release 1
            write_spec_packrel 1
            write_pkgbuild_versions "$VERSION" 1
            echo "Set upstream version to ${VERSION} and packaging release to 1."
            ;;
        s | '')
            echo "Keeping ${VERSION}-${PKGREL}."
            ;;
        *)
            echo "error: invalid release choice '${rel_choice}'." >&2
            exit 1
            ;;
    esac
}

VERSION="$(extract_version)"
PKGREL="$(extract_packaging_release)"

echo "kTile — local package build"
echo "Current upstream version (CMakeLists.txt): ${VERSION}"
echo "Current packaging release (packaging/PACKAGING_RELEASE): ${PKGREL}"

prompt_release_bump

echo
echo "Building as ${VERSION}-${PKGREL} (RPM Release / Debian revision / Arch pkgrel)"
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
    local spec_ver spec_packrel
    spec_ver=$(grep -m1 '^Version:' "${ROOT}/packaging/fedora/ktile.spec" | awk '{print $2}')
    spec_packrel=$(grep -m1 '^%global packrel' "${ROOT}/packaging/fedora/ktile.spec" | awk '{print $3}')
    if [[ -n "$spec_ver" && "$spec_ver" != "$VERSION" ]]; then
        echo "warning: CMake VERSION (${VERSION}) != RPM spec Version (${spec_ver}). Bump the spec before release." >&2
    fi
    if [[ -n "$spec_packrel" && "$spec_packrel" != "${PKGREL}" ]]; then
        echo "warning: packaging/PACKAGING_RELEASE (${PKGREL}) != RPM spec %global packrel (${spec_packrel}). Sync them." >&2
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
    sed -e "s/^%global packrel .*/%global packrel ${PKGREL}/" "$spec_src" >"${top}/SPECS/ktile.spec"
    echo "Running: rpmbuild --define \"_topdir ${top}\" -ba …"
    rpmbuild --define "_topdir ${top}" -ba "${top}/SPECS/ktile.spec"
    echo
    echo "Built RPMs: ${top}/RPMS/"
    echo "Built SRPM: ${top}/SRPMS/"
}

build_deb() {
    need_cmd dpkg-buildpackage
    if ! command -v fakeroot >/dev/null 2>&1; then
        echo "error: fakeroot not found (e.g. apt install fakeroot)." >&2
        exit 1
    fi
    sync_debian_changelog
    # dpkg-buildpackage expects ./debian at the repo root; canonical files live under packaging/debian/.
    # If a plain directory named debian/ already exists, ln would put a link inside it instead of replacing it.
    rm -rf "${ROOT}/debian"
    ln -sfn "${ROOT}/packaging/debian" "${ROOT}/debian"
    if ! deb_changelog_matches; then
        echo "error: packaging/debian/changelog still does not match ktile (${VERSION}-${PKGREL}) after sync." >&2
        exit 1
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
    sed -i "s/^pkgrel=.*/pkgrel=${PKGREL}/" "$pkgbuild"
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
