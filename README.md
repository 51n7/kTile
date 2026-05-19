# kTile 2.0

**kTile** is a snap-windows helper for **KDE Plasma 6**. You define rectangular **regions** on your displays (as fractions of the screen) and assign a **keyboard shortcut** to each. With a window focused, that shortcut moves and resizes it into the matching region—like tiling presets, but using your own layout instead of a fixed grid. You can also open a **region selector** overlay to pick a region visually, or **draw a region** on screen and snap the window to whatever rectangle you define.

<img width="1149" height="933" alt="Screenshot Regions" src="https://github.com/user-attachments/assets/2575b258-5dd1-4228-b343-f62e0393bd83" />

[screencast](https://github.com/user-attachments/assets/5f8ed198-fcc5-4156-af02-c7a826ffb229)

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

**Region selector** — Press the shortcut configured under **Region Selector**. A dimmed fullscreen overlay shows thumbnails of your saved regions; click one to snap the focused window. The optional header includes **Draw region**, **Settings**, and **Close**. Dismiss with **Escape**, **Close**, or a click outside the panel. The overlay auto-closes after the configured idle time unless you set auto-close to 0 (never).

**Draw region** — Press the shortcut configured under **Draw Region** (or the rectangle icon in the region selector header). Drag on the dimmed overlay to define a rectangle; release to snap the focused window. If the drawn area matches a saved region, kTile uses that region’s shortcut; otherwise it applies the custom rectangle. Optional grid lines help align to your **Regions** grid. **Escape** cancels. Only one overlay is active at a time: opening draw region closes the region selector, and vice versa.

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

## Architecture

kTile is split into **three parts** in this repository (one CMake build installs all of them):

| Part | Location | Role |
|------|----------|------|
| **KCM** | `kcm/` | System Settings UI: regions, region selector, draw region, and general shortcuts |
| **KWin script** | `kwin-script/` | Runs in the compositor: snap windows, register global shortcuts, call D-Bus for overlays and settings |
| **Session helper** | `session-helper/` | Autostart app (`ktile-session-helper`): region-picker and draw-region overlays, D-Bus service `org.kde.ktile` |

Overlays are **not** drawn inside KWin or the KCM. The KWin script invokes D-Bus (`showRegionPicker`, `prepareDrawRegion`, `open`, and related methods); the session helper shows the fullscreen UI, then triggers the appropriate shortcut or snap data so KWin moves your **focused** window. That separation keeps the compositor script small and lets overlays handle focus and **Escape** locally (without a permanent global Esc binding that would block KRunner).

After install, `ktile-session-helper` should autostart at login (`.desktop` + D-Bus service). If overlays or “open settings” do nothing, check that the helper is running: `pgrep -a ktile-session-helper`.

## License

GPL-2.0-or-later — see [LICENSE](LICENSE).
