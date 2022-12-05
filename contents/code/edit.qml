import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.LocalStorage 2.15
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

Rectangle {
  property int id
  property var dragging: false
  property int cols: 6
  property int rows: 4
  property int gap: 10
  property int cellWidth: parent.width / cols
  property int cellHeight: parent.height / rows
  property int storeX: 0
  property int storeY: 0

  id: edit
  color: "#EDEDEE"
  width: parent.width
  height: parent.height

  MouseArea {
    anchors.fill: parent
    hoverEnabled: true
    preventStealing: true
    onPositionChanged: {

      var getGridX = Math.floor(mouse.x / cellWidth);
      var getGridY = Math.floor(mouse.y / cellHeight);

      if(!dragging || mouse.x < 0 || mouse.y < 0) {

        // mouse hover
        hoveBox.x = ((cellWidth * getGridX) + (gap / 2))
        hoveBox.y = ((cellHeight * getGridY) + (gap / 2))
        hoveBox.width = (cellWidth - gap)
        hoveBox.height = (cellHeight - gap)
      } else {

        // mouse drag
        var dirextionX = getGridX - storeX
        var dirextionY = getGridY - storeY

        var cellCountX = (dirextionX < 0 ? dirextionX * -1 : dirextionX) + 1
        var cellCountY = (dirextionY < 0 ? dirextionY * -1 : dirextionY) + 1

        if(dirextionX < 1) {
          hoveBox.x = ((cellWidth * getGridX) + (gap / 2))
        }

        if(dirextionY < 1) {
          hoveBox.y = ((cellHeight * getGridY) + (gap / 2))
        }

        hoveBox.width = ((cellWidth * cellCountX) - gap)
        hoveBox.height = ((cellHeight * cellCountY) - gap)

      }
    }
    onClicked: {
      print('onClicked')
    }
    // onPressAndHold: {
    //   print('onPressAndHold')
    // }
    onPressed: {
      dragging = true

      storeX = Math.floor(mouse.x / (parent.width / cols))
      storeY = Math.floor(mouse.y / (parent.height / rows))
    }
    onReleased: {
      dragging = false
    }
  }

  Rectangle {
    id: hoveBox
    width: 100
    height: 100
    color: "red"
  }

  Canvas {
    anchors.fill : parent
    onPaint: {

      var ctx = getContext("2d")
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

  PlasmaComponents.Button {
    icon.name: "dialog-close"
    anchors.top: parent.top
    anchors.right: parent.right
    onClicked: {
      print(id);
      this.parent.destroy()

      // update sqlite row with id and then destroy
    }
  }
}
