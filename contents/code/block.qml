import QtQuick 2.0
import QtQuick.Layouts 1.0
// import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.LocalStorage 2.15
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

Rectangle {
  property int id
  property double boxWidth
  property double boxHeight
  property double boxX
  property double boxY
  
  width: 200
  height: 120
  color: "transparent"
  border.color: "#FFF"
  border.width: 1
  radius: 4

  MouseArea {
    anchors.fill: parent
    hoverEnabled: true
    property bool hovered: false
    onEntered: hovered = true
    onExited: hovered = false

    Button {
      id: hiddenWindowButton
      width: parent.width
      height: parent.height
      anchors.centerIn: parent
      text: "item " + id
      background: Rectangle {
        color: "transparent"
      }
      onClicked: {
        print(id);
      }
    }

    RowLayout {
      // anchors.centerIn: parent
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

  // preview of adjustable space
  Rectangle {
    color: "transparent"
    property double pad: ((1.5 / 100) * parent.width)

    anchors {
      fill: parent
      leftMargin: pad
      rightMargin: pad
      topMargin: pad
      bottomMargin: pad
    }

    Rectangle {
      width: ((boxWidth / 100) * parent.width)
      height: ((boxHeight / 100) * parent.height)
      color: "transparent"
      border.color: "#FFF"
      border.width: 1
      radius: 4
      x: ((boxX / 100) * parent.width)
      y: ((boxY / 100) * parent.height)
    }
  }
}
