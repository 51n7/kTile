// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils
import org.kde.kquickcontrols

// ScrollablePage only wires scrolling reliably when a Flickable is the (first) child; the inner
// MouseArea+implicitHeight fallback does not always size tall content, so the page would not move.
KCMUtils.SimpleKCM {
    id: ktileRoot

    // Match other System Settings pages: tab strip in the page header (below the title), not inside
    // the scrolling body — same pattern as SimpleKCM’s header slot (see kcmutils SimpleKCM.qml).
    header: Column {
        width: parent.width
        spacing: 0

        Item {
            width: parent.width
            height: Kirigami.Units.smallSpacing
        }

        QQC2.TabBar {
            id: ktileTabBar
            position: QQC2.TabBar.Header
            width: parent.width

            QQC2.TabButton {
                text: "Regions"
            }
            QQC2.TabButton {
                text: "General"
            }
            QQC2.TabButton {
                text: "Draw Region"
            }
        }
    }

    property int selectedRegionIndex: -1
    property bool dragActive: false
    property int dragFromIndex: -1
    property int dragInsertIndex: -1
    property real dragYInContent: 0
    property real dragMarkerY: 0
    // insert-before slot 0..regionCount (regionCount = append after last), CodePen-style.
    property int dragInsertBeforeIndex: -1
    property bool dropMarkerAfterLast: false
    property bool importSucceededVisible: false

    // Same accent as drag reorder placeholders (horizontal blue lines).
    readonly property int accentLineStroke: 2
    readonly property int selectionBorderStroke: 1
    readonly property color accentLineColor: Kirigami.Theme.highlightColor
    readonly property int regionListMaxWidth: Kirigami.Units.gridUnit * 52

    function normalizeShortcutText(sequence) {
        let s = String(sequence || "").trim()
        if (!s || /^none(,none)?$/i.test(s) || s === ", ,none") {
            return ""
        }
        while (s.endsWith(",,") && s.includes("+")) {
            s = s.slice(0, -1)
        }
        s = s.replace(/\+Slash/gi, "+/")
        s = s.replace(/\+Question/gi, "+?")
        if (/^slash$/i.test(s)) {
            s = "/"
        }
        if (/^question$/i.test(s)) {
            s = "?"
        }
        return s
    }

    function orderedRegionItems() {
        const cards = []
        const children = regionColumn.children
        for (let i = 0; i < children.length; ++i) {
            const item = children[i]
            if (item && item.hasOwnProperty("isRegionCard") && item.isRegionCard) {
                cards.push(item)
            }
        }
        cards.sort((a, b) => a.regionIndex - b.regionIndex)
        return cards
    }

    // insertAdjacentHTML("beforebegin") slot → QList::move(from, to) (same result as shuffling 0..n-1).
    function insertBeforeToMoveDest(from, insertBefore, n) {
        if (n <= 0) {
            return 0
        }
        let ib = insertBefore
        if (ib < 0) {
            ib = 0
        }
        if (ib > n) {
            ib = n
        }
        if (ib === from || ib === from + 1) {
            return from
        }
        const items = []
        for (let i = 0; i < n; ++i) {
            items.push(i)
        }
        const moved = items.splice(from, 1)[0]
        if (ib === n) {
            items.push(moved)
        } else {
            let pos = ib
            if (from < ib) {
                pos = ib - 1
            }
            items.splice(pos, 0, moved)
        }
        return items.indexOf(moved)
    }

    // CodePen model: pointer over row j = insert *before* that row (cf. .over { border-top }).
    // Y below the list = insert after the last row (slot n).
    function regionHitTest(dropY, fromIndex) {
        const c = orderedRegionItems()
        const n = c.length
        if (n === 0) {
            return {
                dest: 0,
                insertBefore: 0,
                lineAnchorYInRC: 0,
                afterLast: false
            }
        }
        let insertBefore = n
        if (dropY < c[0].cardTopY) {
            insertBefore = 0
        } else {
            for (let j = 0; j < n; ++j) {
                const t = c[j].cardTopY
                const b = c[j].cardBottomY
                if (dropY > b) {
                    continue
                }
                if (dropY < t) {
                    insertBefore = j
                } else {
                    insertBefore = j
                }
                break
            }
        }
        const from = fromIndex >= 0 ? fromIndex : 0
        const dest = insertBeforeToMoveDest(from, insertBefore, n)
        const afterLast = insertBefore === n
        const lineY = afterLast ? c[n - 1].cardBottomY : c[insertBefore].cardTopY
        return {
            dest: dest,
            insertBefore: insertBefore,
            lineAnchorYInRC: lineY,
            afterLast: afterLast
        }
    }

    function updateDropTarget(dropYInRegionColumn) {
        const hit = regionHitTest(dropYInRegionColumn, ktileRoot.dragFromIndex)
        dragYInContent = dropYInRegionColumn
        dragInsertIndex = hit.dest
        dragInsertBeforeIndex = hit.insertBefore
        dropMarkerAfterLast = hit.afterLast
        const p = regionColumn.mapToItem(scrollContent, 0, hit.lineAnchorYInRC)
        dragMarkerY = p.y
    }

    function mapDragPointToRegionColumn(dragSourceItem, lx, ly) {
        const lp = dragSourceItem.mapToItem(regionColumn, lx, ly)
        return lp.y
    }

    Connections {
        target: kcm
        function onRegionsChanged() {
            if (ktileRoot.selectedRegionIndex >= (kcm ? kcm.regions.length : 0)) {
                ktileRoot.selectedRegionIndex = Math.max(0, (kcm ? kcm.regions.length : 1) - 1)
            }
        }
    }

    Flickable {
        id: pageFlick
        width: parent.width
        contentWidth: width
        contentHeight: scrollContent.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: true

        Item {
            id: scrollContent
            width: pageFlick.width
            implicitHeight: mainColumn.implicitHeight + 2 * Kirigami.Units.largeSpacing

            ColumnLayout {
                id: mainColumn
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: Kirigami.Units.largeSpacing
                }
                spacing: Kirigami.Units.largeSpacing

                StackLayout {
                    id: ktileTabStack
                    Layout.fillWidth: true
                    currentIndex: ktileTabBar.currentIndex
                    clip: true

                    ColumnLayout {
                        id: regionsTabColumn
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                Item {
                    id: regionListContainer
                    Layout.fillWidth: true
                    implicitHeight: regionColumn.implicitHeight

                Column {
                    id: regionColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, ktileRoot.regionListMaxWidth)
                    // Inter-row space lives inside each delegate (rowLeadingGap) so the drop line can sit
                    // centered between cards; Column.spacing above the old strip made the gap above the line huge.
                    spacing: 0

                    Repeater {
                        model: kcm ? kcm.regions : []
                        delegate: Item {
                            id: regionItem
                            required property var modelData
                            readonly property bool isRegionCard: true
                            readonly property int regionIndex: modelData.index
                            property bool expanded: true
                            width: regionColumn.width
                            clip: false
                            readonly property int _dropStroke: ktileRoot.accentLineStroke
                            readonly property int _interRowGap: Kirigami.Units.largeSpacing
                            readonly property bool _showInsertSlot: ktileRoot.dragActive && !ktileRoot.dropMarkerAfterLast
                                && ktileRoot.dragInsertBeforeIndex === modelData.index

                            // Between rows: fixed-height band; stroke vertically centered (even space above/below line).
                            // Before first row: same band only while insert-before-slot-0 is active.
                            Item {
                                id: rowLeadingGap
                                z: 10
                                width: parent.width
                                height: (modelData.index > 0 || regionItem._showInsertSlot)
                                    ? regionItem._interRowGap
                                    : 0

                                Rectangle {
                                    visible: regionItem._showInsertSlot
                                    x: 0
                                    y: (parent.height - regionItem._dropStroke) / 2
                                    width: parent.width
                                    height: regionItem._dropStroke
                                    color: ktileRoot.accentLineColor
                                }
                            }

                            readonly property real cardTopY: y + rowLeadingGap.height
                            readonly property real cardHeight: regionCard.height
                            readonly property real cardBottomY: cardTopY + cardHeight
                            implicitHeight: rowLeadingGap.height + regionCard.implicitHeight

                            Item {
                                id: cardFrame
                                anchors.top: rowLeadingGap.bottom
                                width: parent.width
                                implicitHeight: regionCard.implicitHeight

                                Kirigami.Card {
                                    id: regionCard
                                    width: parent.width
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    opacity: (ktileRoot.dragActive && modelData.index === ktileRoot.dragFromIndex) ? 0.45 : 1
                                    contentItem: ColumnLayout {
                                    spacing: Kirigami.Units.smallSpacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        Item {
                                            id: dragHandle
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 1.5
                                            Layout.preferredHeight: Kirigami.Units.gridUnit * 1.5
                                            Layout.alignment: Qt.AlignVCenter
                                            property int dragIndex: modelData.index
                                            Drag.active: dragHandler.active
                                            Drag.source: dragHandle
                                            Drag.hotSpot.x: width / 2
                                            Drag.hotSpot.y: height / 2

                                            Kirigami.Icon {
                                                anchors.centerIn: parent
                                                width: Kirigami.Units.iconSizes.small
                                                height: width
                                                source: "transform-move"
                                                opacity: 0.85
                                            }
                                            DragHandler {
                                                id: dragHandler
                                                target: null
                                                onActiveChanged: {
                                                    if (active) {
                                                        ktileRoot.dragActive = true
                                                        ktileRoot.dragFromIndex = modelData.index
                                                        const yRc = ktileRoot.mapDragPointToRegionColumn(
                                                            dragHandle,
                                                            width / 2,
                                                            height / 2)
                                                        ktileRoot.updateDropTarget(yRc)
                                                        return
                                                    }
                                                    const from = ktileRoot.dragFromIndex
                                                    const dest = ktileRoot.dragInsertIndex
                                                    ktileRoot.dragActive = false
                                                    ktileRoot.dragFromIndex = -1
                                                    ktileRoot.dragInsertIndex = -1
                                                    ktileRoot.dragInsertBeforeIndex = -1
                                                    ktileRoot.dropMarkerAfterLast = false
                                                    ktileRoot.dragMarkerY = 0
                                                    if (from >= 0 && dest >= 0 && from !== dest) {
                                                        kcm.moveRegion(from, dest)
                                                        ktileRoot.selectedRegionIndex = -1
                                                    }
                                                }
                                                onCentroidChanged: {
                                                    if (!active) {
                                                        return
                                                    }
                                                    const yRc = ktileRoot.mapDragPointToRegionColumn(
                                                        dragHandle,
                                                        centroid.position.x,
                                                        centroid.position.y)
                                                    ktileRoot.updateDropTarget(yRc)
                                                }
                                            }
                                        }

                                        QQC2.ToolButton {
                                            Layout.alignment: Qt.AlignVCenter
                                            icon.name: regionItem.expanded ? "arrow-down" : "arrow-right"
                                            onClicked: {
                                                regionItem.expanded = !regionItem.expanded
                                                ktileRoot.selectedRegionIndex = modelData.index
                                            }
                                        }

                                        Item {
                                            Layout.alignment: Qt.AlignVCenter
                                            implicitWidth: regionTitleLabel.implicitWidth
                                            implicitHeight: regionTitleLabel.implicitHeight

                                            QQC2.Label {
                                                id: regionTitleLabel
                                                text: "Region " + modelData.number
                                                font.bold: true
                                            }

                                            MouseArea {
                                                anchors.fill: regionTitleLabel
                                                acceptedButtons: Qt.LeftButton
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: ktileRoot.selectedRegionIndex = modelData.index
                                            }
                                        }

                                        Item {
                                            id: regionHeaderSpacer
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                                            implicitHeight: Math.max(
                                                regionTitleLabel.implicitHeight,
                                                regionShortcut.implicitHeight)

                                            MouseArea {
                                                anchors.fill: parent
                                                acceptedButtons: Qt.LeftButton
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: ktileRoot.selectedRegionIndex = modelData.index
                                            }
                                        }

                                        RowLayout {
                                            Layout.alignment: Qt.AlignVCenter
                                            spacing: Kirigami.Units.smallSpacing

                                            QQC2.ComboBox {
                                                visible: kcm && kcm.displaySelectorVisible
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.preferredWidth: implicitWidth
                                                Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                                                Layout.minimumWidth: Kirigami.Units.gridUnit * 7
                                                model: kcm ? kcm.screenChoices : []
                                                currentIndex: {
                                                    if (!kcm || count < 1) {
                                                        return 0
                                                    }
                                                    return Math.min(Math.max(0, modelData.display + 1), count - 1)
                                                }
                                                onActivated: function (index) {
                                                    if (kcm) {
                                                        kcm.setDisplayValue(modelData.index, index - 1)
                                                    }
                                                }
                                            }

                                            Item {
                                                Layout.alignment: Qt.AlignVCenter
                                                implicitWidth: regionShortcut.implicitWidth
                                                implicitHeight: regionShortcut.implicitHeight

                                                KeySequenceItem {
                                                    id: regionShortcut
                                                    anchors.fill: parent
                                                    keySequence: modelData.shortcut
                                                    onKeySequenceModified: {
                                                        const normalized = ktileRoot.normalizeShortcutText(keySequence)
                                                        if (normalized.length > 0) {
                                                            kcm.setShortcutValue(modelData.index, normalized)
                                                        } else {
                                                            kcm.setShortcutValue(modelData.index, "")
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    RegionGridEditor {
                                        Layout.fillWidth: true
                                        visible: regionItem.expanded
                                        regionIndex: modelData.index
                                        region: modelData.region
                                    }
                                }
                                }

                                Rectangle {
                                    visible: modelData.index === ktileRoot.selectedRegionIndex
                                    anchors.fill: parent
                                    anchors.margins: -ktileRoot.selectionBorderStroke
                                    radius: Kirigami.Units.cornerRadius + ktileRoot.selectionBorderStroke
                                    color: "transparent"
                                    border.width: ktileRoot.selectionBorderStroke
                                    border.color: ktileRoot.accentLineColor
                                    antialiasing: false
                                    z: 1
                                    enabled: false
                                }
                            }
                        }
                    }
                }
                }
                    }

                    ColumnLayout {
                        id: generalTabColumn
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Open kTile Settings:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            KeySequenceItem {
                                Layout.alignment: Qt.AlignVCenter
                                keySequence: kcm ? kcm.openSettingsShortcut : ""
                                onKeySequenceModified: {
                                    if (!kcm) {
                                        return
                                    }
                                    const normalized = ktileRoot.normalizeShortcutText(keySequence)
                                    kcm.openSettingsShortcut = normalized.length > 0 ? normalized : ""
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Open Region Selector:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            KeySequenceItem {
                                Layout.alignment: Qt.AlignVCenter
                                keySequence: kcm ? kcm.openRegionPickerShortcut : ""
                                onKeySequenceModified: {
                                    if (!kcm) {
                                        return
                                    }
                                    const normalized = ktileRoot.normalizeShortcutText(keySequence)
                                    kcm.openRegionPickerShortcut = normalized.length > 0 ? normalized : ""
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Region Selector Overlay Opacity:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2

                                RowLayout {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Kirigami.Units.smallSpacing

                                    QQC2.Slider {
                                        id: regionPickerOpacitySlider
                                        Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                                        from: 0
                                        to: 100
                                        stepSize: 5
                                        live: true
                                        enabled: kcm !== null
                                        value: kcm ? Math.round(kcm.regionPickerOverlayOpacity * 100) : 30
                                        onMoved: {
                                            if (kcm) {
                                                kcm.regionPickerOverlayOpacity = value / 100
                                            }
                                        }
                                        onPressedChanged: {
                                            if (!pressed && kcm) {
                                                kcm.regionPickerOverlayOpacity = value / 100
                                            }
                                        }
                                    }

                                    QQC2.Label {
                                        text: kcm ? (Math.round(kcm.regionPickerOverlayOpacity * 100) + "%") : "30%"
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Show Region Selector Header:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            QQC2.CheckBox {
                                Layout.alignment: Qt.AlignVCenter
                                enabled: kcm !== null
                                checked: kcm ? kcm.regionPickerShowHeader : true
                                onToggled: {
                                    if (kcm) {
                                        kcm.regionPickerShowHeader = checked
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing
                            visible: kcm && kcm.displaySelectorVisible

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Move window to next screen:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            KeySequenceItem {
                                Layout.alignment: Qt.AlignVCenter
                                keySequence: kcm ? kcm.moveToNextScreenShortcut : ""
                                onKeySequenceModified: {
                                    if (!kcm) {
                                        return
                                    }
                                    const normalized = ktileRoot.normalizeShortcutText(keySequence)
                                    kcm.moveToNextScreenShortcut = normalized.length > 0 ? normalized : ""
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Export/Import:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            QQC2.Button {
                                Layout.alignment: Qt.AlignVCenter
                                icon.name: "document-export"
                                text: "Export..."
                                onClicked: exportSettingsDialog.open()
                            }

                            QQC2.Button {
                                Layout.alignment: Qt.AlignVCenter
                                icon.name: "document-import"
                                text: "Import..."
                                onClicked: importSettingsDialog.open()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing
                            visible: ktileRoot.importSucceededVisible

                            Kirigami.Icon {
                                Layout.alignment: Qt.AlignVCenter
                                source: "dialog-ok-apply"
                                width: Kirigami.Units.iconSizes.small
                                height: width
                                isMask: true
                                color: Kirigami.Theme.positiveTextColor
                            }

                            QQC2.Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                wrapMode: Text.Wrap
                                text: "Settings imported successfully."
                                color: Kirigami.Theme.positiveTextColor
                            }
                        }
                    }

                    ColumnLayout {
                        id: drawRegionTabColumn
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Draw region on screen:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            KeySequenceItem {
                                Layout.alignment: Qt.AlignVCenter
                                keySequence: kcm ? kcm.openDrawRegionShortcut : ""
                                onKeySequenceModified: {
                                    if (!kcm) {
                                        return
                                    }
                                    const normalized = ktileRoot.normalizeShortcutText(keySequence)
                                    kcm.openDrawRegionShortcut = normalized.length > 0 ? normalized : ""
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Draw Region Overlay Opacity:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            QQC2.Slider {
                                id: drawRegionOpacitySlider
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                                from: 0
                                to: 100
                                stepSize: 5
                                live: true
                                enabled: kcm !== null
                                value: kcm ? Math.round(kcm.drawRegionOverlayOpacity * 100) : 30
                                onMoved: {
                                    if (kcm) {
                                        kcm.drawRegionOverlayOpacity = value / 100
                                    }
                                }
                                onPressedChanged: {
                                    if (!pressed && kcm) {
                                        kcm.drawRegionOverlayOpacity = value / 100
                                    }
                                }
                            }

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: kcm ? (Math.round(kcm.drawRegionOverlayOpacity * 100) + "%") : "30%"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                Layout.alignment: Qt.AlignVCenter
                                text: "Show grid lines:"
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            }

                            QQC2.CheckBox {
                                Layout.alignment: Qt.AlignVCenter
                                enabled: kcm !== null
                                checked: kcm ? kcm.drawRegionShowGridLines : false
                                onToggled: {
                                    if (kcm) {
                                        kcm.drawRegionShowGridLines = checked
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // "After last" slot only (CodePen has no row below; use a line under the list). Rows
            // 0..n-1 use border-top on the delegate (see regionItem) like .over { border-top: 2px… }.
            Item {
                readonly property int lineHeight: 2
                readonly property int hInset: Kirigami.Units.smallSpacing
                readonly property int gapAboveLine: 2
                readonly property int gapUnderLine: 2
                readonly property point _colOrigin: regionColumn.mapToItem(scrollContent, 0, 0)
                x: _colOrigin.x + hInset
                width: regionColumn.width - 2 * hInset
                height: lineHeight + gapUnderLine
                y: ktileRoot.dragMarkerY + gapAboveLine
                z: 1000
                visible: ktileRoot.dragActive && ktileRoot.dragFromIndex >= 0 && ktileRoot.dropMarkerAfterLast

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: parent.lineHeight
                    radius: 1
                    color: ktileRoot.accentLineColor
                }
            }
        }
    }

    footer: ColumnLayout {
        spacing: 0

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerRow.implicitHeight + Kirigami.Units.smallSpacing * 2
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.35)

            RowLayout {
                id: footerRow
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: "Grid"
                    elide: Text.ElideRight
                }

                QQC2.SpinBox {
                    id: gridColsSpin
                    from: 1
                    to: 64
                    editable: true
                    value: kcm.gridColumns
                    onValueModified: {
                        if (kcm !== null && value !== kcm.gridColumns) {
                            kcm.gridColumns = value
                        }
                    }
                }

                QQC2.Label {
                    text: "×"
                }

                QQC2.SpinBox {
                    id: gridRowsSpin
                    from: 1
                    to: 64
                    editable: true
                    value: kcm.gridRows
                    onValueModified: {
                        if (kcm !== null && value !== kcm.gridRows) {
                            kcm.gridRows = value
                        }
                    }
                }

                QQC2.Label {
                    text: "|"
                    opacity: 0.35
                }

                QQC2.Label {
                    text: "Gap"
                }

                QQC2.SpinBox {
                    id: gridGapSpin
                    from: 0
                    to: 48
                    editable: true
                    value: kcm.gridGap
                    onValueModified: {
                        if (kcm !== null && value !== kcm.gridGap) {
                            kcm.gridGap = value
                        }
                    }
                }

                Connections {
                    target: kcm
                    enabled: kcm !== null
                    function onGridLayoutChanged() {
                        gridColsSpin.value = kcm.gridColumns
                        gridRowsSpin.value = kcm.gridRows
                        gridGapSpin.value = kcm.gridGap
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                QQC2.Button {
                    icon.name: "edit-delete"
                    text: "Remove Region"
                    enabled: kcm && kcm.regions.length > 1
                        && ktileRoot.selectedRegionIndex >= 0
                        && ktileRoot.selectedRegionIndex < kcm.regions.length
                    onClicked: {
                        const i = ktileRoot.selectedRegionIndex
                        if (i < 0 || !kcm || kcm.regions.length <= 1) {
                            return
                        }
                        kcm.removeRegion(i)
                        const n = kcm.regions.length
                        ktileRoot.selectedRegionIndex = n > 0 ? Math.min(i, n - 1) : -1
                    }
                }

                QQC2.Button {
                    icon.name: "list-add"
                    text: "Add Region"
                    onClicked: kcm.addRegion()
                }
            }
        }
    }

    FileDialog {
        id: exportSettingsDialog
        title: "Export kTile settings"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: {
            const err = kcm.exportSettingsToUrl(selectedFile)
            if (err.length > 0) {
                importExportErrorDialog.dialogTitle = "Export failed"
                importExportErrorDialog.message = err
                importExportErrorDialog.open()
            }
        }
    }

    FileDialog {
        id: importSettingsDialog
        title: "Import kTile settings"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: {
            const err = kcm.importSettingsFromUrl(selectedFile)
            if (err.length > 0) {
                ktileRoot.importSucceededVisible = false
                importExportErrorDialog.dialogTitle = "Import failed"
                importExportErrorDialog.message = err
                importExportErrorDialog.open()
            } else {
                ktileRoot.importSucceededVisible = true
                importSuccessTimer.restart()
            }
        }
    }

    Timer {
        id: importSuccessTimer
        interval: 4000
        repeat: false
        onTriggered: ktileRoot.importSucceededVisible = false
    }

    QQC2.Dialog {
        id: importExportErrorDialog
        property string dialogTitle: ""
        property string message: ""

        title: dialogTitle.length > 0 ? dialogTitle : "kTile"
        modal: true
        dim: true
        standardButtons: QQC2.Dialog.Ok
        width: Math.min(Kirigami.Units.gridUnit * 28, ktileRoot.width > 0 ? ktileRoot.width * 0.9 : Kirigami.Units.gridUnit * 28)
        parent: ktileRoot
        anchors.centerIn: parent

        contentItem: QQC2.Label {
            width: importExportErrorDialog.width - 2 * Kirigami.Units.largeSpacing
            wrapMode: Text.Wrap
            text: importExportErrorDialog.message
        }
    }
}
