# kTile 2.0

**kTile** snaps windows to custom screen regions on **KDE Plasma 6**. Each region gets a **shortcut**; press it with a window focused to move and resize that window there. You can also pick a region from a [preview modal](https://github.com/user-attachments/assets/aa00b119%2Da4d3%2D47e0%2D9acd%2D18664e808a57) or [draw a region](https://github.com/user-attachments/assets/48919155%2De5ef%2D4ea9%2Dae4f%2D5b39a6c8a79e) on screen.

[View Demo](https://github.com/user-attachments/assets/5f8ed198%2Dfcc5%2D4156%2Daf02%2Dc7a826ffb229) 👀

<img width="1149" height="933" alt="Screenshot Regions" src="https://github.com/user-attachments/assets/2575b258-5dd1-4228-b343-f62e0393bd83" />

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

Open **System Settings → Window Management → kTile**. Settings are grouped into four tabs:

- **Regions** — Region list, per-region shortcuts, and grid editor for layout and snap targets
- **Region Selector** — Global shortcut, overlay opacity, header visibility, and auto-close timeout (0 = never)
- **Draw Region** — Shortcut to open on-screen selection, overlay opacity, grid lines, and auto-close timeout (0 = never)
- **General** — Shortcuts for **Open kTile Settings** and **Move window to next screen**, plus export/import of all kTile settings

<table>
    <tr>
        <td width="50%">
            <a href="https://github.com/user-attachments/assets/2575b258-5dd1-4228-b343-f62e0393bd83">
                <img width="794" height="282" alt="Screenshot_20260518_140005-thumb" src="https://github.com/user-attachments/assets/628118d8-8f65-4bb6-bf44-f5747476d944" />
            </a>
        </td>
        <td width="50%">
            <a href="https://github.com/user-attachments/assets/6c106aaf-9fb2-42b2-ae78-b5e710feace1">
                <img width="794" height="282" alt="Screenshot_20260518_140021-thumb" src="https://github.com/user-attachments/assets/27e0faa5-6d7a-474f-b3e0-a3ba1e5a7a05" />
            </a>
        </td>
    </tr>
    <tr>
        <td width="50%" align="center"><strong>Regions</strong></td>
        <td width="50%" align="center"><strong>Region Selector</strong></td>
    </tr>
</table>

<table>
    <tr>
        <td width="50%">
            <a href="https://github.com/user-attachments/assets/de741c75-d9ec-430f-9463-82dbf1ba863d">
                <img width="794" height="282" alt="Screenshot_20260518_140034-thumb" src="https://github.com/user-attachments/assets/bcea5f5e-e4af-4f6b-9323-ad198be12bd4" />
            </a>
        </td>
        <td width="50%">
            <a href="https://github.com/user-attachments/assets/ff426aa9-55c5-468a-aff8-9a55407e1c16">
                <img width="794" height="282" alt="Screenshot_20260518_140044-thumb" src="https://github.com/user-attachments/assets/9dc8b30a-2f8e-46dd-a8e9-0939c3680eb6" />
            </a>
        </td>
    </tr>
    <tr>
        <td width="50%" align="center"><strong>Draw Region</strong></td>
        <td width="50%" align="center"><strong>General</strong></td>
    </tr>
</table>

Regions and overlay options are stored in `~/.config/kwinrc` under `[Script-org.kde.ktile]`; the session helper reads the same keys for the picker and draw-region overlays.

## Usage

**Direct snap** — Focus a normal window and press a region’s shortcut to move and resize it into that region.

**Region selector** — Press the shortcut configured under **Region Selector**. A dimmed fullscreen overlay shows thumbnails of your saved regions; click one to snap the focused window. The overlay auto-closes after the configured idle time unless you set auto-close to 0 (never).

**Draw region** — Press the shortcut configured under **Draw Region** (or the rectangle icon in the region selector header). Drag on the dimmed overlay to define a rectangle; release to snap the focused window.

<table>
    <tr>
        <td width="50%">
            <a href="https://github.com/user-attachments/assets/51bda717-0445-4fbd-9f46-8467fd79029d">
                <img width="794" height="282" alt="Screenshot_20260518_154829-thumb" src="https://github.com/user-attachments/assets/6162e724-c20f-4efb-9f09-ad6c9dfb64ff" />
               <!-- <img width="1040" height="563" alt="Screenshot_20260518_154829-small" src="https://github.com/user-attachments/assets/aa00b119-a4d3-47e0-9acd-18664e808a57" /> -->
            </a>
        </td>
        <td width="50%">
            <a href="https://github.com/user-attachments/assets/a6fbdd28-86ba-4801-94a9-4ddf0c13cf50">
                <img width="794" height="282" alt="Screenshot_20260518_154933-thumb" src="https://github.com/user-attachments/assets/48919155-e5ef-4ea9-ae4f-5b39a6c8a79e" />
            </a>
        </td>
    </tr>
    <tr>
        <td width="50%" align="center"><strong>Region Selector</strong></td>
        <td width="50%" align="center"><strong>Draw Region</strong></td>
    </tr>
</table>

<!-- [screencast.webm](https://github.com/user-attachments/assets/dad767cd-8ef5-4cc4-9a18-262eb9eaa43e) -->

If behavior looks wrong after install or upgrade, enable **kTile** again under **System Settings → Window Management → KWin Scripts**, ensure **ktile-session-helper** is running (`pgrep -a ktile-session-helper`), or sign out and back in once.

## Packaging

See [PACKAGING.md](PACKAGING.md) to build installable packages on your machine (Fedora-style RPM, Debian `.deb`, or Arch), install the packaging tools for your distro, then run:

```bash
./build.sh
```

The script is interactive: it picks the package type, creates the source tarball where needed, and runs `rpmbuild`, `dpkg-buildpackage`, or `makepkg`. You do not need a remote repository to package locally (manual tarball + spec steps are also documented in `PACKAGING.md` if you prefer not to use `build.sh`).

## License

GPL-2.0-or-later — see [LICENSE](LICENSE).
