import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.12
import QtQuick.LocalStorage 2.15
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

PlasmaComponents.Button {
  property int id
  property double boxWidth
  property double boxHeight
  property double boxX
  property double boxY
  
  width: 200
  height: 120

  function tileWindow(window) {
    if (!window.normalWindow) return;

    let screen = workspace.clientArea(KWin.MaximizeArea, workspace.activeScreen, window.desktop);
    
    let newWidth = ((boxWidth / 100) * screen.width)
    let newHeight = ((boxHeight / 100) * screen.height)
    let newX = ((boxX / 100) * screen.width)
    let newY = ((boxY / 100) * screen.height)

    window.setMaximize(false, false);
    window.geometry = Qt.rect(newX, newY, newWidth, newHeight);
  }

  MouseArea {
    property double pad: ((2 / 100) * parent.width) // 2% of parent.width
    property bool hovered: false
    onEntered: hovered = true
    onExited: hovered = false

    anchors.margins: pad
    anchors.fill: parent
    hoverEnabled: true

    onClicked: {
      tileWindow(workspace.activeClient);
      mainDialog.visible = false
    }

    PlasmaComponents.Button {
      width: ((boxWidth / 100) * parent.width)
      height: ((boxHeight / 100) * parent.height)
      x: ((boxX / 100) * parent.width)
      y: ((boxY / 100) * parent.height)
      // enabled: false
      // opacity: 1
      // background: Rectangle {
      //   color: "red"
      // }
    }

    RowLayout {
      anchors.top: parent.top
      anchors.right: parent.right
      visible: parent.hovered

      PlasmaComponents.Button {
        icon.name: "edit-entry"
        onClicked: {
          print('edit');

          var component = Qt.createComponent("edit.qml")
          component.createObject(tableBackground, {id: id});
        }
      }

      PlasmaComponents.Button {
        icon.name: "edit-delete-symbolic"
        onClicked: {
          print('delete');

          this.parent.parent.parent.destroy()

          var db = LocalStorage.openDatabaseSync("QDeclarativeExampleDB", "1.0", "The Example QML SQL!", 1000000);

          db.transaction(
            function(tx) {
              tx.executeSql('DELETE FROM spaces WHERE rowid = ' + id);
            }
          )
        }
      }
    }
  }
}
