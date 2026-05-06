# kTile

**Repository:** https://github.com/51n7/kTile

Snap the active window to a custom rectangle on KDE Plasma 6 (KWin script + **System Settings** KCM). Not a replacement window manager; separate from KWin’s built-in tiling.

**Project context for contributors/agents:** see [AGENTS.md](AGENTS.md), including a **Progress** section on recent KCM work (accordion regions, drag reorder, footer, drop marker behavior).

## Status

The **Window Management → kTile** page supports multiple regions, native shortcut picking per region, collapsible cards, **drag-to-reorder** (with a visible drop line), and **Add Region** in a footer strip above the system Apply/Defaults row. Technical notes and pitfalls for the next change are in [AGENTS.md](AGENTS.md).

## Quick install (Fedora / source)

1. Install build dependencies (see **PACKAGING.md** or the message from `./install-kcm.sh` if `cmake` is missing).
2. Run:

   ```bash
   ./install-kcm.sh
   ```

   For `~/.local` installs, the script writes `~/.config/plasma-workspace/env/ktile-paths.sh` automatically.
   Log out and back in once so System Settings launched from the app menu sees the KCM.

3. In **System Settings → Window Management → KWin Scripts**, enable **kTile**.
4. Configure **Window Management → kTile** and set shortcuts under **Shortcuts** (default: Meta+Shift+1).

## Packaging for other users

See [PACKAGING.md](PACKAGING.md) (Fedora RPM spec under `packaging/fedora/`, COPR notes, why Flatpak is a poor fit).

You do not need a remote repository to package locally: create a tarball from the git checkout and build a package from that tarball (documented in `PACKAGING.md`).

## License

GPL-2.0-or-later — see [LICENSE](LICENSE).
