import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.LocalStorage 2.15
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

Rectangle {
  property int id
  property var dragging: false
  property int cols: 8
  property int rows: 6
  property double storeX: 0
  property double storeY: 0
  property double previewWidth: 0
  property double previewHeight: 0
  property double previewX: 0
  property double previewY: 0
  property double setWidth: 0
  property double setHeight: 0
  property double setX: 0
  property double setY: 0
  property string backgroundColor: "#5f5f5f"
  property string selectionColor: "#ffffff"
  property string hoverColor: "#4996ff"
  property double setDisplay: 0

  id: edit
  color: "transparent"
  anchors.fill: parent
  anchors.margins: 6

  ColumnLayout {
    anchors.fill: parent

    RowLayout {

      SpinBox {
        value: cols
        onValueChanged: {
          cols = value
          lines.requestPaint()
        }
      }

      PlasmaComponents.Label {
        text: "x"
      }

      SpinBox {
        value: rows
        onValueChanged: {
          rows = value
          lines.requestPaint()
        }
      }

      PlasmaComponents.Label {
        text: "|"
      }

      SpinBox {
        value: gap
        to: 24
        onValueChanged: {
          gap = value
        }
      }

      PlasmaComponents.Label {
        text: "|"
        visible: screenList.length > 2
      }

      ComboBox {
        id: displayCombo
        width: 200
        model: screenList
        visible: screenList.length > 2
        property bool isInitializing: true

        onCurrentIndexChanged: {
          if (!isInitializing) {
            setDisplay = displayCombo.currentIndex
          }
        }
        
        Component.onCompleted: {
          displayCombo.currentIndex = setDisplay;
          isInitializing = false;
        }
      }

      PlasmaComponents.Button {
        Layout.fillWidth: true
        enabled: false
        opacity: 0
      }
      
      PlasmaComponents.Button {
        icon.name: "checkbox"
        onClicked: {
          var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);
          var preview = flowLayout.children[(id - 1)]

          if(
            previewWidth !== 0 ||
            previewHeight !== 0 ||
            previewX !== 0 ||
            previewY !== 0
            ) {
            
            preview.boxWidth = (100 * previewWidth) / grid.width
            preview.boxHeight = (100 * previewHeight) / grid.height
            preview.boxX = (100 * previewX) / grid.width
            preview.boxY = (100 * previewY) / grid.height
            
            db.transaction(
              function(tx) {
                tx.executeSql('UPDATE spaces2 SET width = '+ preview.boxWidth +', height = '+ preview.boxHeight +', x = '+ preview.boxX +', y = '+ preview.boxY +' WHERE rowid = ' + id);
              }
            )
          }

          db.transaction(
            function(tx) {
              tx.executeSql('UPDATE spaces2 SET display = '+ setDisplay +' WHERE rowid = ' + id);
            }
          )

          db.transaction(
            function(tx) {
              tx.executeSql('UPDATE grid SET x = '+ cols +', y = '+ rows +', gap = '+ gap +' WHERE rowid = 1');
            }
          )

          preview.displayNum = setDisplay
          this.parent.parent.parent.destroy()
          mainColumnLayout.visible = true;
        }
      }
      
      PlasmaComponents.Button {
        icon.name: "edit-redo"
        onClicked: {
          this.parent.parent.parent.destroy()
          mainColumnLayout.visible = true;
        }
      }

      PlasmaComponents.Button {
        icon.name: "dialog-close"
        onClicked: {
          this.parent.parent.parent.destroy()
          mainColumnLayout.visible = true;
          mainDialog.visible = false;
        }
      }
    }

    Rectangle {
      Layout.fillWidth: true
      Layout.fillHeight: true
      color: backgroundColor
      
      MouseArea {
        id: grid
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        property double cellWidth: parent.width / cols
        property double cellHeight: parent.height / rows

        onPositionChanged: {

          var getGridX = Math.floor(mouse.x / cellWidth);
          var getGridY = Math.floor(mouse.y / cellHeight);

          // keep MouseArea values in bounds
          // there's some weirdness with mouse position being tracked while dragging outside component
          if(
            mouse.x >= 0 &&
            mouse.x <= parent.width &&
            mouse.y >= 0 &&
            mouse.y <= parent.height &&
            getGridX < cols &&
            getGridY < rows
          ) {
            if(!dragging) {

              // mouse hover
              hoverBox.width = (cellWidth - gap)
              hoverBox.height = (cellHeight - gap)
              hoverBox.x = ((cellWidth * getGridX) + (gap / 2))
              hoverBox.y = ((cellHeight * getGridY) + (gap / 2))
              
            } else {

              // mouse drag
              var dirextionX = getGridX - storeX
              var dirextionY = getGridY - storeY

              var cellCountX = (dirextionX < 0 ? dirextionX * -1 : dirextionX) + 1
              var cellCountY = (dirextionY < 0 ? dirextionY * -1 : dirextionY) + 1

              if(dirextionX < 1) {
                hoverBox.x = ((cellWidth * getGridX) + (gap / 2))
                previewX = (cellWidth * getGridX)
              }

              if(dirextionY < 1) {
                hoverBox.y = ((cellHeight * getGridY) + (gap / 2))
                previewY = (cellHeight * getGridY)
              }

              hoverBox.width = ((cellWidth * cellCountX) - gap)
              hoverBox.height = ((cellHeight * cellCountY) - gap)

              previewWidth = (cellWidth * cellCountX)
              previewHeight = (cellHeight * cellCountY)

            }
          }
        }
        onPressed: {
          dragging = true

          storeX = Math.floor(mouse.x / (parent.width / cols))
          storeY = Math.floor(mouse.y / (parent.height / rows))
        }
        onReleased: {
          dragging = false
          
          shapePreview.width = hoverBox.width
          shapePreview.height = hoverBox.height
          shapePreview.x = hoverBox.x
          shapePreview.y = hoverBox.y
        }
      }

      Rectangle {
        id: shapePreview
        width: ((setWidth / 100) * parent.width) - gap
        height: ((setHeight / 100) * parent.height) - gap
        x: ((setX / 100) * parent.width) + (gap / 2)
        y: ((setY / 100) * parent.height) + (gap / 2)
        color: selectionColor
      }

      Rectangle {
        id: hoverBox
        width: 0
        height: 0
        color: hoverColor
      }

      Canvas {
        property double cellWidth: parent.width / cols
        property double cellHeight: parent.height / rows
        id: lines
        anchors.fill : parent
        visible: true
        onPaint: {

          var ctx = getContext("2d")
          ctx.reset()
          ctx.lineWidth = 1
          ctx.strokeStyle = "black"
          ctx.beginPath()

          var nrows = parent.height/cellHeight;
          for(var i=0; i < nrows+1; i++){
            ctx.moveTo(0, cellHeight*i);
            ctx.lineTo(parent.width, cellHeight*i);
          }

          var ncols = parent.width/cellWidth
          for(var j=0; j < ncols+1; j++){
            ctx.moveTo(cellWidth*j, 0);
            ctx.lineTo(cellWidth*j, parent.height);
          }

          ctx.closePath()
          ctx.stroke()
        }
      }
    }
  }

  Component.onCompleted: {

    var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

    db.transaction(
      function(tx) {
        let rs = tx.executeSql('SELECT rowid, * FROM grid');
        cols = rs.rows[0].x
        rows = rs.rows[0].y
        gap = rs.rows[0].gap
      }
    )
  }
}