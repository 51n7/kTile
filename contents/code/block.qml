import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.LocalStorage 2.15

Rectangle {
  property int id
  width: 200
  height: 120
  color: "transparent"
  border.color: "#b1dc1c"
  border.width: 1
  radius: 5

  Button {
    id: hiddenWindowButton
    // width: parent.width
    // height: parent.height
    anchors.centerIn: parent
    visible: !editMode
    text: "item " + id
    // background: Rectangle {
    //     color: "transparent"
    // }
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
          var test = tx.executeSql('DELETE FROM e_spaces WHERE rowid = ' + id);
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
