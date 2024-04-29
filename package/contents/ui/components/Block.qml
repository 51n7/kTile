import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kwin
import QtQuick.LocalStorage

PlasmaComponents.Button {
  property var block: {}
  width: 200
  height: 120

  MouseArea {
    property double pad: ((2 / 100) * parent.width) // 2% of parent.width
    property bool isHovered: false
    onEntered: isHovered = true
    onExited: isHovered = false
    anchors.margins: pad
    anchors.fill: parent
    hoverEnabled: true

    onClicked: {
      tileWindow(Workspace.activeWindow, block.id);
      mainDialog.visible = false
    }

    PlasmaComponents.Button {
      width: ((block.boxWidth / 100) * parent.width)
      height: ((block.boxHeight / 100) * parent.height)
      x: ((block.boxX / 100) * parent.width)
      y: ((block.boxY / 100) * parent.height)
      visible: !parent.isHovered
    }

    // fake the button hover state
    PlasmaComponents.Button {
      width: ((block.boxWidth / 100) * parent.width)
      height: ((block.boxHeight / 100) * parent.height)
      x: ((block.boxX / 100) * parent.width)
      y: ((block.boxY / 100) * parent.height)
      visible: parent.isHovered
      signal hovered()

      MouseArea {
        anchors.fill: parent

        onClicked: {
          tileWindow(Workspace.activeWindow, block.id);
          mainDialog.visible = false
        }
      }
    }

    RowLayout {
      anchors.top: parent.top
      anchors.right: parent.right
      visible: parent.isHovered

      PlasmaComponents.Button {
        icon.name: "edit-entry"
        onClicked: {
          // var component = Qt.createComponent("edit.qml")
          // component.createObject(mainDialog, {
          //   id: id,
          //   setWidth: boxWidth,
          //   setHeight: boxHeight,
          //   setX: boxX,
          //   setY: boxY,
          //   setDisplay: displayNum
          // });
          // mainColumnLayout.visible = false;
        }
      }

      PlasmaComponents.Button {
        icon.name: "edit-delete-symbolic"
        onClicked: {

          // this.parent.parent.parent.destroy()

          // var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);
          
          // db.transaction(
          //   function(tx) {

          //     // rebuild Flow blocks - I dont love it..
          //     tx.executeSql('DELETE FROM spaces2 WHERE rowid = ' + block.id);
          //     tx.executeSql('UPDATE sqlite_sequence SET seq = 0')
          //     flowLayout.children = "";

          //     var rs = tx.executeSql('SELECT rowid, * FROM spaces2');
          //     for (var i = 0; i < rs.rows.length; i++) {
          //       tx.executeSql('UPDATE spaces2 SET rowid = ' + (i + 1) + ' WHERE rowid = ' + rs.rows.item(i).rowid)
          //       reDraw((i + 1), rs.rows.item(i).width, rs.rows.item(i).height, rs.rows.item(i).x, rs.rows.item(i).y, rs.rows.item(i).display)
          //     }
          //   }
          // )
        }
      }
    }
  }

  Text {
    text: block.id
    font.pointSize: 38
    anchors.centerIn: parent
    opacity: 0.2
    visible: showNumbers
  }
}