// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property int regionIndex
    required property double boxWidth
    required property double boxHeight
    required property double boxX
    required property double boxY

    signal activated(int regionIndex)
    signal pressed()

    width: 200
    height: 120

    readonly property color frameColor: palette.button
    readonly property color borderNormal: palette.mid
    readonly property color borderHover: palette.highlight

    Rectangle {
        id: outerFrame
        anchors.fill: parent
        color: root.frameColor
        radius: 4
        border.width: 1
        border.color: hoverArea.containsMouse ? root.borderHover : root.borderNormal
    }

    MouseArea {
        id: hoverArea
        readonly property real pad: root.width * 0.02
        anchors.fill: parent
        anchors.margins: pad
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: root.pressed()
        onClicked: root.activated(root.regionIndex)

        Rectangle {
            width: (root.boxWidth / 100) * parent.width
            height: (root.boxHeight / 100) * parent.height
            x: (root.boxX / 100) * parent.width
            y: (root.boxY / 100) * parent.height
            color: root.frameColor
            radius: 2
            border.width: 1
            border.color: hoverArea.containsMouse ? root.borderHover : root.borderNormal
        }
    }
}
