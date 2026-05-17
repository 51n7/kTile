// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls

/**
 * Full-screen grid selector (same percent model as kcm/ui/RegionGridEditor.qml).
 */
Item {
    id: root

    required property int cols
    required property int rows
    required property int gap
    property bool showGridLines: false

    /** Grid percentages (0–100), same strings as KCM region slots — see RegionGridEditor.qml. */
    signal regionSelected(real xPct, real yPct, real wPct, real hPct)

    property bool dragging: false
    property int anchorGX: 0
    property int anchorGY: 0
    property int dragGX0: 0
    property int dragGY0: 0
    property int dragGX1: 0
    property int dragGY1: 0
    property bool hasSelection: false

    function cellMetrics() {
        const w = Math.max(1, gridArea.width)
        const h = Math.max(1, gridArea.height)
        return {
            cw: w / cols,
            ch: h / rows
        }
    }

    function posToCell(mx, my) {
        const m = cellMetrics()
        const gx = Math.floor(mx / m.cw)
        const gy = Math.floor(my / m.ch)
        return {
            gx: Math.max(0, Math.min(cols - 1, gx)),
            gy: Math.max(0, Math.min(rows - 1, gy))
        }
    }

    function placeBox(box, gx0, gy0, gx1, gy1) {
        const m = cellMetrics()
        const a0 = Math.min(gx0, gx1)
        const a1 = Math.max(gx0, gx1)
        const b0 = Math.min(gy0, gy1)
        const b1 = Math.max(gy0, gy1)
        const half = gap / 2
        box.x = a0 * m.cw + half
        box.y = b0 * m.ch + half
        box.width = Math.max(0, (a1 - a0 + 1) * m.cw - gap)
        box.height = Math.max(0, (b1 - b0 + 1) * m.ch - gap)
    }

    function commitCells(gx0, gy0, gx1, gy1) {
        const a0 = Math.min(gx0, gx1)
        const a1 = Math.max(gx0, gx1)
        const b0 = Math.min(gy0, gy1)
        const b1 = Math.max(gy0, gy1)
        const xPct = (a0 / cols) * 100
        const yPct = (b0 / rows) * 100
        const wPct = ((a1 - a0 + 1) / cols) * 100
        const hPct = ((b1 - b0 + 1) / rows) * 100
        hasSelection = true
        placeBox(selectionRect, a0, b0, a1, b1)
        regionSelected(xPct, yPct, wPct, hPct)
    }

    function clearSelection() {
        dragging = false
        hasSelection = false
        hoverBox.visible = false
        selectionRect.width = 0
        selectionRect.height = 0
    }

    Rectangle {
        id: gridArea
        anchors.fill: parent
        color: "transparent"
        clip: true

        onWidthChanged: lines.requestPaint()
        onHeightChanged: lines.requestPaint()

        Rectangle {
            id: selectionRect
            z: 1
            color: Qt.alpha(palette.highlight, 0.35)
            border.color: palette.highlight
            border.width: 2
            visible: hasSelection && !root.dragging
        }

        Rectangle {
            id: hoverBox
            z: 2
            visible: false
            color: Qt.alpha(palette.highlight, 0.22)
            border.color: Qt.alpha(palette.highlight, 0.6)
            border.width: 1
        }

        Canvas {
            id: lines
            z: 3
            anchors.fill: parent
            enabled: false
            visible: root.showGridLines
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineWidth = 1
                ctx.strokeStyle = Qt.rgba(255, 255, 255, 0.45)
                const m = root.cellMetrics()
                const W = width
                const H = height
                ctx.beginPath()
                for (let i = 0; i <= rows; ++i) {
                    ctx.moveTo(0, m.ch * i)
                    ctx.lineTo(W, m.ch * i)
                }
                for (let j = 0; j <= cols; ++j) {
                    ctx.moveTo(m.cw * j, 0)
                    ctx.lineTo(m.cw * j, H)
                }
                ctx.stroke()
            }
        }

        MouseArea {
            id: gridMouse
            z: 4
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            onPositionChanged: function (mouse) {
                if (mouse.x < 0 || mouse.x > parent.width || mouse.y < 0 || mouse.y > parent.height) {
                    return
                }
                const p = root.posToCell(mouse.x, mouse.y)
                if (!root.dragging) {
                    hoverBox.visible = containsMouse
                    root.placeBox(hoverBox, p.gx, p.gy, p.gx, p.gy)
                } else {
                    hoverBox.visible = true
                    const gx0 = root.anchorGX
                    const gy0 = root.anchorGY
                    root.dragGX0 = Math.min(gx0, p.gx)
                    root.dragGY0 = Math.min(gy0, p.gy)
                    root.dragGX1 = Math.max(gx0, p.gx)
                    root.dragGY1 = Math.max(gy0, p.gy)
                    root.placeBox(hoverBox, gx0, gy0, p.gx, p.gy)
                }
            }
            onPressed: function (mouse) {
                root.dragging = true
                const p = root.posToCell(mouse.x, mouse.y)
                root.anchorGX = p.gx
                root.anchorGY = p.gy
                root.dragGX0 = root.dragGX1 = p.gx
                root.dragGY0 = root.dragGY1 = p.gy
                hoverBox.visible = true
                root.placeBox(hoverBox, p.gx, p.gy, p.gx, p.gy)
            }
            onReleased: function () {
                root.dragging = false
                if (hoverBox.width <= 0 || hoverBox.height <= 0) {
                    hoverBox.visible = false
                    return
                }
                root.commitCells(root.dragGX0, root.dragGY0, root.dragGX1, root.dragGY1)
                hoverBox.visible = containsMouse
            }
            onExited: {
                if (!root.dragging) {
                    hoverBox.visible = false
                }
            }
        }
    }

    onColsChanged: lines.requestPaint()
    onRowsChanged: lines.requestPaint()
    onGapChanged: lines.requestPaint()
    onShowGridLinesChanged: {
        if (showGridLines) {
            lines.requestPaint()
        }
    }
}
