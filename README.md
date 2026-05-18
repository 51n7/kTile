# kTile 2.0

**kTile** is a snap-windows helper for **KDE Plasma 6**. You define rectangular **regions** on your displays (as fractions of the screen) and assign a **keyboard shortcut** to each. While a window is focused, pressing that shortcut moves and resizes the window into the matching region, similar to tiling presets, but driven by your own layout instead of a fixed grid.

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

Open **System Settings → Window Management → kTile**. There you can:

- Add and remove desktop **regions**
- Reorder regions (**drag-and-drop**)
- Edit each **region** on a **grid** (columns, rows, and gap define how the rectangle maps to the active screen)
- Assign a **shortcut per region** using the standard key-sequence picker
- In the **General** tab, optionally set shortcuts for **opening kTile settings** and **opening the region picker** (visual grid overlay)

Regions are stored in `~/.config/kwinrc` under `[Script-org.kde.ktile]`; the session helper reads the same keys for the picker preview.

<img width="1060" height="629" alt="Screenshot General" src="https://github.com/user-attachments/assets/89adc006-c173-4a5f-b8ab-4fe89288e8c2" />

## Usage

**Direct snap:** Focus a normal window and press a region’s shortcut to snap it into that region.

**Region picker:** Press the “open region picker” shortcut (General tab). A dimmed overlay shows region thumbnails; click one to snap the active window. Dismiss with **Escape**, the panel **close** button, or a click on the dimmed area outside the panel.

If behavior looks wrong after install or upgrade, enable **kTile** again under **System Settings → Window Management → KWin Scripts**, ensure **ktile-session-helper** is running, or sign out and back in once.

**Duplicate shortcuts in Keyboard settings** (each `kTile: …` action listed twice) can happen when old `kglobalshortcutsrc` entries stack on top of the KWin script. Run `./scripts/purge-ktile-shortcuts.sh`, reinstall or reload the KWin script, then open the kTile KCM and click **Apply** once.

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
| **KCM** | `kcm/` | System Settings UI: edit regions, shortcuts, grid layout |
| **KWin script** | `kwin-script/` | Runs in the compositor: snap windows, register global shortcuts, call D-Bus to open the picker |
| **Session helper** | `session-helper/` | Small autostart app (`ktile-session-helper`): fullscreen region-picker overlay, session D-Bus service `org.kde.ktile` |

The picker is **not** drawn inside KWin or the KCM. The KWin script only invokes `showRegionPicker` over D-Bus; the session helper shows the dimmed overlay and thumbnails, then triggers the chosen region shortcut so KWin snaps your **focused** window. That separation keeps the compositor script small and lets the overlay handle focus and **Escape** locally (without a permanent global Esc binding that would block KRunner).

After install, `ktile-session-helper` should autostart at login (`.desktop` + D-Bus service). If the picker or “open settings” shortcut does nothing, check that the helper is running: `pgrep -a ktile-session-helper`.

## License

GPL-2.0-or-later — see [LICENSE](LICENSE).

## Examples

[Screencast](https://github.com/user-attachments/assets/bf2ab0af-93f8-4ff2-9a5b-3bc546033659)

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
        <td width="50%">Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt.</td>
        <td width="50%">Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt.</td>
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
        <td width="50%">Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt.</td>
        <td width="50%">Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt.</td>
    </tr>
</table>












