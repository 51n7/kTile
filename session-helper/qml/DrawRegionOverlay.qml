// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls

Window {
    id: drawWindow
    objectName: "ktileDrawRegionWindow"

    property var controller: drawRegionController

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    modality: Qt.ApplicationModal
    title: "kTile Draw Region"
    color: "transparent"

    function closeOverlay() {
        if (controller) {
            controller.closeOverlay()
        }
    }

    function focusEscapeTrap() {
        raise()
        requestActivate()
        escapeTrap.forceActiveFocus(Qt.OtherFocusReason)
    }

    onVisibleChanged: {
        if (visible) {
            regionGrid.clearSelection()
            Qt.callLater(focusEscapeTrap)
        } else {
            if (controller) {
                controller.cancelAutoCloseTimer()
            }
            regionGrid.clearSelection()
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
                    drawWindow.closeOverlay()
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
                    if (!escapeTrap.activeFocus && drawWindow.visible) {
                        Qt.callLater(drawWindow.focusEscapeTrap)
                    }
                }
            }
        }

        Rectangle {
            id: backdrop
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, controller ? controller.overlayOpacity : 0.30)
        }

        DrawRegionGrid {
            id: regionGrid
            z: 1
            anchors.fill: parent
            cols: controller ? controller.gridColumns : 8
            rows: controller ? controller.gridRows : 6
            gap: controller ? controller.gridGap : 0
            showGridLines: controller ? controller.showGridLines : false

            onDrawStarted: {
                if (controller) {
                    controller.cancelAutoCloseTimer()
                }
            }

            onRegionSelected: function (xPct, yPct, wPct, hPct) {
                if (controller) {
                    controller.snapToPercentRect(xPct, yPct, wPct, hPct)
                }
            }
        }
    }
}
