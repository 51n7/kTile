// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: pickerWindow
    objectName: "ktileRegionPickerWindow"

    property var controller: pickerController

    readonly property int cellWidth: 200
    readonly property int cellHeight: 120
    readonly property int gridSpacing: 10
    readonly property int chromeMargins: 24
    readonly property int headerHeight: 28
    readonly property int gridColumns: 4
    readonly property int regionCount: controller ? controller.regions.length : 0
    readonly property int panelWidth: chromeMargins * 2 + gridColumns * cellWidth
                                            + (gridColumns - 1) * gridSpacing
    readonly property bool showPickerHeader: controller ? controller.showPickerHeader : true
    readonly property int panelScrollHeight: 362
    readonly property int panelHeight: chromeMargins * 2 + panelScrollHeight
                                         + (showPickerHeader ? headerHeight + 6 : 0)

    // No BypassWindowManagerHint: compositor must be allowed to give this window keyboard focus.
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    modality: Qt.ApplicationModal
    title: "kTile Region Picker"
    color: "transparent"

    function fillScreen() {
        const screen = Qt.application.screens.length > 0 ? Qt.application.screens[0] : null
        if (!screen) {
            return
        }
        x = screen.virtualX
        y = screen.virtualY
        width = screen.width
        height = screen.height
    }

    function closePicker() {
        if (controller) {
            controller.closePicker()
        }
    }

    function focusEscapeTrap() {
        raise()
        requestActivate()
        escapeTrap.forceActiveFocus(Qt.OtherFocusReason)
    }

    onVisibleChanged: {
        if (visible) {
            fillScreen()
            Qt.callLater(focusEscapeTrap)
        }
    }

    onActiveChanged: {
        if (active && visible) {
            Qt.callLater(focusEscapeTrap)
        }
    }

    FocusScope {
        id: focusScope
        anchors.fill: parent
        focus: true

        // Invisible focus target so Escape reaches this window without a global shortcut.
        TextField {
            id: escapeTrap
            objectName: "ktileEscapeTrap"
            z: 0
            x: 0
            y: 0
            width: 1
            height: 1
            opacity: 0.01
            color: "transparent"
            selectionColor: "transparent"
            selectedTextColor: "transparent"
            placeholderTextColor: "transparent"
            placeholderText: ""
            maximumLength: 1
            text: "\u200b"
            background: Item {}

            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Escape) {
                    event.accepted = true
                    pickerWindow.closePicker()
                }
            }

            onTextChanged: {
                if (text !== "\u200b") {
                    text = "\u200b"
                }
            }

            Connections {
                target: escapeTrap
                function onActiveFocusChanged() {
                    if (!escapeTrap.activeFocus && pickerWindow.visible && !pickerPanel.pointerHeld) {
                        Qt.callLater(pickerWindow.focusEscapeTrap)
                    }
                }
            }
        }

        // Dimmed full-screen backdrop (click outside the panel to dismiss).
        Rectangle {
            id: backdrop
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, controller ? controller.overlayOpacity : 0.30)

            MouseArea {
                anchors.fill: parent
                hoverEnabled: false
                onPressed: function (mouse) {
                    mouse.accepted = true
                    pickerWindow.closePicker()
                }
            }
        }

        Rectangle {
            id: pickerPanel
            z: 1
            anchors.centerIn: parent
            width: pickerWindow.panelWidth
            height: pickerWindow.panelHeight
            color: palette.window
            border.color: palette.mid
            radius: 6

            property bool pointerHeld: false

            // Block backdrop clicks on panel chrome; do not compete with header controls.
            MouseArea {
                z: -1
                anchors.fill: parent
                hoverEnabled: false
                onPressed: function (mouse) {
                    mouse.accepted = true
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: showPickerHeader ? headerHeight : 0
                    visible: showPickerHeader
                    spacing: 2

                    Label {
                        text: "kTile"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        font.bold: true
                        font.pixelSize: 13
                    }

                    ToolButton {
                        Layout.preferredWidth: headerHeight
                        Layout.preferredHeight: headerHeight
                        padding: 4
                        focusPolicy: Qt.NoFocus
                        icon.name: "settings-configure"
                        display: AbstractButton.IconOnly
                        ToolTip.visible: hovered
                        ToolTip.text: "Open kTile settings"
                        onPressed: pickerPanel.pointerHeld = true
                        onReleased: pickerPanel.pointerHeld = false
                        onCanceled: pickerPanel.pointerHeld = false
                        onClicked: {
                            if (controller) {
                                controller.openSettings()
                            }
                        }
                    }

                    ToolButton {
                        Layout.preferredWidth: headerHeight
                        Layout.preferredHeight: headerHeight
                        padding: 4
                        focusPolicy: Qt.NoFocus
                        icon.name: "dialog-close"
                        display: AbstractButton.IconOnly
                        ToolTip.visible: hovered
                        ToolTip.text: "Close"
                        onPressed: pickerPanel.pointerHeld = true
                        onReleased: pickerPanel.pointerHeld = false
                        onCanceled: pickerPanel.pointerHeld = false
                        onClicked: pickerWindow.closePicker()
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    GridLayout {
                        id: regionGrid
                        width: implicitWidth
                        columns: pickerWindow.gridColumns
                        rowSpacing: pickerWindow.gridSpacing
                        columnSpacing: pickerWindow.gridSpacing

                        Repeater {
                            model: controller ? controller.regions : []
                            delegate: RegionBlock {
                                required property var modelData
                                required property int index

                                Layout.preferredWidth: pickerWindow.cellWidth
                                Layout.preferredHeight: pickerWindow.cellHeight
                                Layout.row: Math.floor(index / pickerWindow.gridColumns)
                                Layout.column: index % pickerWindow.gridColumns

                                regionIndex: modelData.index
                                boxX: modelData.boxX
                                boxY: modelData.boxY
                                boxWidth: modelData.boxWidth
                                boxHeight: modelData.boxHeight
                                onActivated: function (regionIndex) {
                                    if (controller) {
                                        controller.snapToRegion(regionIndex)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
