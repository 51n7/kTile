# kTile

This is a Kwin script that allows you to create a desktop region by grid selection and then let you quickly snap the window to that positon.

![Screenshot_20221216_123040](https://user-images.githubusercontent.com/2657818/208176885-579e686c-7660-4eae-a91c-4d6d7f3d57a2.png)

![Screenshot_20221216_123429](https://user-images.githubusercontent.com/2657818/208176916-75d1e906-c494-4bf8-b4c6-abc486a87512.png)

## Installation

- Clone repo to the following directory then log out and back in:
```
~/.local/share/kwin/scripts/
```
- You will then see the newly added script in `System Settings -> Window Management -> KWin Scripts`
- Select the kTile script and hit `Apply`:

![Screenshot_20221216_125031](https://user-images.githubusercontent.com/2657818/208177924-d9fe174b-3d93-4901-8663-f51af5411239.png)

## Usage

By default the script uses `Ctrl + .` to bring up the main window, to change this see the configuration section below.
In the main window you can click the ➕ button in the top right to add a new region then hover over the new region to see the edit and delete options.
In edit mode you can then drag select to draw the size/position you want and also set the grid and gap you need.
Once you're done hit the ✔️ button to save.

## Configuration

Most of the configuration is done through the graphical interface however due to Kwin limitations shortcuts have to be predefined but can be updated in `System Settings -> Shortcuts -> KWin`.

**Note that I've added a visibility button (main window top right) to toggle numbers on each region block to easily connect it with the Kwin shortcuts.**

![Screenshot_20221216_124805](https://user-images.githubusercontent.com/2657818/208177894-e6365378-6ea2-4216-b7c8-e89afb5c7ccd.png)

## Technical Notes

- This application is written 100% in QML.
- A core part of this script relies on [QML Local Storage](https://doc.qt.io/qt-6/qtquick-localstorage-qmlmodule.html) to allow using custom UI to save settings. I would love to have the settings interated into KDE's configuration window but currently that doesnt seem possible as it only accpets basic form controls through XML.

As this is my first time writing a Kwin script and using QML I realize there is a lot for me to refactor here. Any feedback would be much appreciated by creating a new [issue](https://github.com/jonbestdev/kTile/issues).

### TODO:
- [x] ~~save custom grid size~~
- [x] ~~bug with sqlite not reindexing table~~
- [x] ~~allow gap resize~~
- [ ] close with escape
- [ ] close by clicking outside window
- [ ] quick full screen drag selection

## Special Thanks

This would not be possible with out inspiration from [Moom](https://manytricks.com/moom/) and [Exquisite](https://github.com/qewer33/Exquisite), so many many thanks to both of these great apps.







