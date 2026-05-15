# kTile 2.0

**kTile** is a snap-windows helper for **KDE Plasma 6**. You define rectangular **regions** on your displays (as fractions of the screen) and assign a **keyboard shortcut** to each. While a window is focused, pressing that shortcut moves and resizes the window into the matching region, similar to tiling presets, but driven by your own layout instead of a fixed grid.

<img width="1060" height="1051" alt="Screenshot Regions" src="https://github.com/user-attachments/assets/8f35bb13-316f-4849-9578-ec385762c9e5" />

## Quick install

1. Grab your build file from the `dist` folder
2. Run:

   ```bash
   # Fedora
   sudo dnf install ~/dist/ktile-*.rpm

   # Debian
   sudo dpkg -i ~/dist/ktile-*.deb

   # Arch
   sudo pacman -U ~/dist/ktile-*.pkg.tar.zst
   ```

   You might need to log out and back in once so System Settings launched from the app menu sees the kTile (KCM) settings.

## Configuration

Open **System Settings → Window Management → kTile**. There you can:

- Add and remove desktop **regions**
- Reorder regions (**drag-and-drop**)
- Edit each **region** on a **grid** (columns, rows, and gap define how the rectangle maps to the active screen)
- Assign a **shortcut per region** using the standard key-sequence picker
- Optionally set shortcut for **opening kTile settings** in the General tab

<img width="1060" height="629" alt="Screenshot General" src="https://github.com/user-attachments/assets/89adc006-c173-4a5f-b8ab-4fe89288e8c2" />

## Usage

Focus a normal window and press a region’s shortcut to snap it into that region.

If behavior looks wrong after install or upgrade, enable **kTile** again under **System Settings → Window Management → KWin Scripts**, or sign out and back in once.

## Packaging

See [PACKAGING.md](PACKAGING.md) to build installable packages on your machine (Fedora-style RPM, Debian `.deb`, or Arch), install the packaging tools for your distro, then run:

```bash
./build.sh
```

The script is interactive: it picks the package type, creates the source tarball where needed, and runs `rpmbuild`, `dpkg-buildpackage`, or `makepkg`. You do not need a remote repository to package locally (manual tarball + spec steps are also documented in `PACKAGING.md` if you prefer not to use `build.sh`).

## License

GPL-2.0-or-later — see [LICENSE](LICENSE).
