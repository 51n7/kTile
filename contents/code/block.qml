import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.LocalStorage 2.15

Rectangle {
  property int id
  property int boxWidth
  property int boxHeight
  property int boxX
  property int boxY
  width: 200
  height: 120
  color: "transparent"
  border.color: "#b1dc1c"
  border.width: 1
  radius: 4

  Rectangle {
    color: "transparent"

    anchors {
      fill: parent
      leftMargin: 4
      rightMargin: 4
      topMargin: 4
      bottomMargin: 4
    }

    Rectangle {
      width: ((boxWidth / 100) * parent.width)
      height: ((boxHeight / 100) * parent.height)
      color: "transparent"
      border.color: "#b1dc1c"
      border.width: 1
      radius: 4
      x: ((boxX / 100) * parent.width)
      y: ((boxY / 100) * parent.height)
    }
  }

  Button {
    id: hiddenWindowButton
    width: parent.width
    height: parent.height
    anchors.centerIn: parent
    visible: !editMode
    text: "item " + id
    background: Rectangle {
      color: "transparent"
    }
    onClicked: {
      print(id);
    }
  }

  Button {
    text: "delete me"
    // background: Rectangle {
    //     color: "transparent"
    // }
    anchors.top: parent.top
    anchors.right: parent.right
    visible: editMode
    onClicked: {
      print('delete me');
      this.parent.destroy()

      var db = LocalStorage.openDatabaseSync("QDeclarativeExampleDB", "1.0", "The Example QML SQL!", 1000000);

      db.transaction(
        function(tx) {
          var test = tx.executeSql('DELETE FROM spaces WHERE rowid = ' + id);
          // print(test.rows[0].rowid);
        }
      )
    }
  }

  Button {
    text: "edit me"
    anchors.top: parent.top
    anchors.left: parent.left
    visible: editMode
    onClicked: {
      print('edit me');

      var component = Qt.createComponent("edit.qml")
      component.createObject(tableBackground, {id: id});
    }
  }
}
