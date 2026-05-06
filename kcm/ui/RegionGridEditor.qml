// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

/**
 * Maps the virtual desktop rectangle (kcm.virtualGeometry) onto a cols×rows grid.
 * Drag to select a rectangle; the region is saved as "x y width height" in global pixels
 * (same as the KWin script).
 */
Item {
    id: root
    required property int regionIndex
    required property string region

    implicitHeight: Kirigami.Units.gridUnit * 16

    readonly property int cols: kcm ? kcm.gridColumns : 8
    readonly property int rows: kcm ? kcm.gridRows : 6
    readonly property int gap: kcm ? kcm.gridGap : 0
    readonly property rect vg: kcm ? kcm.virtualGeometry : Qt.rect(0, 0, 1920, 1080)

    property bool dragging: false
    property int anchorGX: 0
    property int anchorGY: 0
    property int dragGX0: 0
    property int dragGY0: 0
    property int dragGX1: 0
    property int dragGY1: 0
    property int selGX0: 0
    property int selGY0: 0
    property int selGX1: 0
    property int selGY1: 0
    property bool hasSelection: false

    function cellMetrics() {
        const w = Math.max(1, gridArea.width)
        const h = Math.max(1, gridArea.height)
        return {
            cw: w / cols,
            ch: h / rows
        }
    }

    function boundaries(origin, span, count) {
        const out = []
        for (let i = 0; i <= count; ++i) {
            out.push(Math.round(origin + (i * span) / count))
        }
        return out
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

    function commitCells(gx0, gy0, gx1, gy1) {
        const a0 = Math.min(gx0, gx1)
        const a1 = Math.max(gx0, gx1)
        const b0 = Math.min(gy0, gy1)
        const b1 = Math.max(gy0, gy1)
        const g = vg
        if (!kcm || g.width <= 0 || g.height <= 0) {
            return
        }
        const bx = boundaries(g.x, g.width, cols)
        const by = boundaries(g.y, g.height, rows)
        const x0 = bx[a0]
        const y0 = by[b0]
        const x1 = bx[a1 + 1]
        const y1 = by[b1 + 1]
        const w = Math.max(1, x1 - x0)
        const h = Math.max(1, y1 - y0)
        const x = x0
        const y = y0
        // Keep the visual selection in sync immediately; model update follows.
        hasSelection = true
        placeBox(selectionRect, a0, b0, a1, b1)
        kcm.setRegionValue(regionIndex, x + " " + y + " " + w + " " + h)
    }

    function applyRegionString(spec) {
        const parts = String(spec).trim().split(/\s+/)
        if (parts.length < 4) {
            hasSelection = false
            return
        }
        const x = Number(parts[0])
        const y = Number(parts[1])
        const w = Number(parts[2])
        const h = Number(parts[3])
        if (![x, y, w, h].every((n) => isFinite(n)) || w <= 0 || h <= 0) {
            hasSelection = false
            return
        }
        const g = vg
        if (g.width <= 0 || g.height <= 0) {
            return
        }
        const bx = boundaries(g.x, g.width, cols)
        const by = boundaries(g.y, g.height, rows)
        const rx0 = x
        const ry0 = y
        const rx1 = x + w
        const ry1 = y + h
        let gx0 = cols
        let gy0 = rows
        let gx1 = -1
        let gy1 = -1
        for (let i = 0; i < cols; ++i) {
            const c0 = bx[i]
            const c1 = bx[i + 1]
            if (Math.max(c0, rx0) < Math.min(c1, rx1)) {
                gx0 = Math.min(gx0, i)
                gx1 = Math.max(gx1, i)
            }
        }
        for (let j = 0; j < rows; ++j) {
            const r0 = by[j]
            const r1 = by[j + 1]
            if (Math.max(r0, ry0) < Math.min(r1, ry1)) {
                gy0 = Math.min(gy0, j)
                gy1 = Math.max(gy1, j)
            }
        }
        if (gx1 < 0 || gy1 < 0) {
            hasSelection = false
            return
        }
        gx0 = Math.max(0, Math.min(cols - 1, gx0))
        gy0 = Math.max(0, Math.min(rows - 1, gy0))
        gx1 = Math.max(0, Math.min(cols - 1, gx1))
        gy1 = Math.max(0, Math.min(rows - 1, gy1))
        if (gx1 < gx0 || gy1 < gy0) {
            hasSelection = false
            return
        }
        selGX0 = gx0
        selGY0 = gy0
        selGX1 = gx1
        selGY1 = gy1
        hasSelection = true
        placeBox(selectionRect, gx0, gy0, gx1, gy1)
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

    function refreshSelection() {
        applyRegionString(region)
    }

    onRegionChanged: refreshSelection()

    Component.onCompleted: refreshSelection()
    onVisibleChanged: if (visible) {
        refreshSelection()
    }
    onWidthChanged: refreshSelection()
    onHeightChanged: refreshSelection()

    Connections {
        target: kcm
        enabled: kcm !== null
        function onVirtualGeometryChanged() {
            refreshSelection()
        }
        function onGridLayoutChanged() {
            refreshSelection()
        }
    }

    Rectangle {
        id: gridArea
        anchors.fill: parent
        color: Qt.darker(Kirigami.Theme.backgroundColor, 1.12)
        clip: true
        onWidthChanged: {
            lines.requestPaint()
            refreshSelection()
        }
        onHeightChanged: {
            lines.requestPaint()
            refreshSelection()
        }

        // Fills sit under grid lines so cell boundaries stay visible on the selection.
        Rectangle {
            id: selectionRect
            z: 1
            color: Kirigami.Theme.highlightColor
            visible: hasSelection && !root.dragging
        }

        Rectangle {
            id: hoverBox
            z: 2
            visible: false
            color: Qt.alpha(Kirigami.Theme.highlightColor, 0.55)
        }

        Canvas {
            id: lines
            z: 3
            anchors.fill: parent
            enabled: false
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineWidth = 1
                ctx.strokeStyle = Qt.rgba(0, 0, 0, 0.35)
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
                const m = root.cellMetrics()
                const cw = m.cw
                const ch = m.ch
                if (mouse.x < 0 || mouse.x > parent.width || mouse.y < 0 || mouse.y > parent.height) {
                    return
                }
                const p = root.posToCell(mouse.x, mouse.y)
                const getGridX = p.gx
                const getGridY = p.gy
                if (!root.dragging) {
                    hoverBox.visible = containsMouse
                    root.placeBox(hoverBox, getGridX, getGridY, getGridX, getGridY)
                } else {
                    hoverBox.visible = true
                    let gx0 = root.anchorGX
                    let gy0 = root.anchorGY
                    let gx1 = getGridX
                    let gy1 = getGridY
                    root.dragGX0 = Math.min(gx0, gx1)
                    root.dragGY0 = Math.min(gy0, gy1)
                    root.dragGX1 = Math.max(gx0, gx1)
                    root.dragGY1 = Math.max(gy0, gy1)
                    root.placeBox(hoverBox, gx0, gy0, gx1, gy1)
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
            onReleased: {
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

    onColsChanged: {
        lines.requestPaint()
        refreshSelection()
    }
    onRowsChanged: {
        lines.requestPaint()
        refreshSelection()
    }
    onGapChanged: refreshSelection()
}
