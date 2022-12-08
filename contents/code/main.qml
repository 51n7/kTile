import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kwin 2.0
import QtQuick.LocalStorage 2.15

PlasmaCore.Dialog {
  id: mainDialog

  location: PlasmaCore.Types.Floating
  flags: Qt.X11BypassWindowManagerHint | Qt.FramelessWindowHint
  visible: false

  // property int columns: 6
  // property int rows: 4
  property bool editMode: false
  property bool restartButtonVisible: true

  function loadConfig(){
    // columns = KWin.readConfig("columns", 6);
    // rows = KWin.readConfig("rows", 4);
  }

  function show() {
    var screen = workspace.clientArea(KWin.FullScreenArea, workspace.activeScreen, workspace.currentDesktop);
    mainDialog.visible = true;

    mainDialog.x = screen.x + screen.width/2 - mainDialog.width/2;
    mainDialog.y = screen.y + screen.height/2 - mainDialog.height/2;
  }

  ColumnLayout {
    id: mainColumnLayout
    anchors.horizontalCenter: parent.horizontalCenter

    RowLayout {
      id: headerRowLayout
      // visible: mainDialog.headerVisible

      PlasmaComponents.Label {
        text: "kTile"
        Layout.fillWidth: true
      }

      /*Switch {
        id: editToggle
        onToggled: editMode = !editMode
        indicator: Rectangle {
          implicitWidth: 48
          implicitHeight: 26
          x: editToggle.width - width - editToggle.rightPadding
          y: parent.height / 2 - height / 2
          radius: 13
          color: editToggle.checked ? "green" : "red"
          // border.color: "black"

          Rectangle {
            x: editToggle.checked ? parent.width - width : 0
            width: 26
            height: 26
            radius: 13
            // border.color: "black"
          }
        }
      }*/
      
      PlasmaComponents.Button {
        icon.name: "list-add-symbolic"
        visible: restartButtonVisible
        onClicked: {
          print('add');

          var db = LocalStorage.openDatabaseSync("QDeclarativeExampleDB", "1.0", "The Example QML SQL!", 1000000);

          db.transaction(
            function(tx) {

              tx.executeSql('CREATE TABLE IF NOT EXISTS spaces(width INTEGER, height INTEGER, x INTEGER, y INTEGER)');
              // tx.executeSql('DROP TABLE spaces');
              
              // var rs = tx.executeSql('SELECT rowid FROM spaces ORDER BY ROWID DESC LIMIT 1');
              var insert = tx.executeSql('INSERT INTO spaces VALUES(?, ?, ?, ?) RETURNING rowid', [ 50, 50, 0, 0 ]);

              print(insert.rows[0].rowid)

              var component = Qt.createComponent("block.qml")
              var object = component.createObject(flowLayout, {
                id: insert.rows[0].rowid,
                boxWidth: 50,
                boxHeight: 50,
                boxX: 0,
                boxY: 0
              });
            }
          )
        }
      }

      /*PlasmaComponents.Button {
        icon.name: "edit-entry"
        visible: restartButtonVisible
        onClicked: {
          print('edit');
        }
      }
      
      PlasmaComponents.Button {
        icon.name: "edit-delete-symbolic"
        visible: restartButtonVisible
        onClicked: {
          print('delete');
        }
      }*/
      
      PlasmaComponents.Button {
        icon.name: "dialog-close"
        onClicked: {
          mainDialog.visible = false;
        }
      }
    }

    Rectangle {
      id: tableBackground
      color: "transparent"
      // border.width: 2
      // border.color: "#EDEDEE"
      // radius: 4
      width: 895
      height: 400

      ScrollView {
        id: tableArea
        anchors.fill: parent
        clip: true

        anchors {
          fill: parent
          leftMargin: 30
          rightMargin: 0
          topMargin: 10
          bottomMargin: 10
        }
        
        Flow {
          property int rowCount: parent.width / (200 + spacing)
          property int rowWidth: rowCount * 200 + (rowCount - 1) * spacing
          property int mar: (parent.width - rowWidth) / 2

          id: flowLayout
          width: 900
          spacing: 10

          Component.onCompleted: {
            
            var component = Qt.createComponent("block.qml")

            var db = LocalStorage.openDatabaseSync("QDeclarativeExampleDB", "1.0", "The Example QML SQL!", 1000000);

            db.transaction(
              function(tx) {
                var rs = tx.executeSql('SELECT rowid, * FROM spaces');
                for (var i = 0; i < rs.rows.length; i++) {
                  var object = component.createObject(flowLayout, {
                    id: rs.rows.item(i).rowid,
                    boxWidth: rs.rows.item(i).width,
                    boxHeight: rs.rows.item(i).height,
                    boxX: rs.rows.item(i).x,
                    boxY: rs.rows.item(i).y
                  });
                }
              }
            )
          }
        }
      }
    }
  }

  Component.onCompleted: {
    KWin.registerWindow(mainDialog);
    KWin.registerShortcut(
      "kTile",
      "kTile",
      "Ctrl+.",
      function() {
        if (mainDialog.visible) {
          mainDialog.visible = false;
        } else {
          mainDialog.loadConfig();
          mainDialog.show();
        }
      }
    );

    mainDialog.loadConfig();
  }
}
