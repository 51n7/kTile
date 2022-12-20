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

  property bool editMode: false
  property bool showNumbers: false
  property double gap: 10
  property string database: "ktile"

  function show() {
    var screen = workspace.clientArea(KWin.FullScreenArea, workspace.activeScreen, workspace.currentDesktop);
    mainDialog.visible = true;

    mainDialog.x = screen.x + screen.width/2 - mainDialog.width/2;
    mainDialog.y = screen.y + screen.height/2 - mainDialog.height/2;
  }

  function tileWindow(window, pos) {

    if (!window.normalWindow) return;
    let screen = workspace.clientArea(KWin.MaximizeArea, workspace.activeScreen, window.desktop);
    let db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

    db.transaction(
      function(tx) {
        const rs = tx.executeSql('SELECT rowid, * FROM spaces WHERE rowid = ' + pos);

        let newWidth = ((rs.rows[0].width / 100) * (screen.width - gap)) - gap
        let newHeight = ((rs.rows[0].height / 100) * (screen.height - gap)) - gap
        let newX = ((rs.rows[0].x / 100) * (screen.width - gap)) + gap
        let newY = ((rs.rows[0].y / 100) * (screen.height - gap)) + gap

        window.setMaximize(false, false);
        window.geometry = Qt.rect(newX, newY, newWidth, newHeight);
      }
    )
  }

  function reDraw(id, width, height, x, y) {
    var component = Qt.createComponent("block.qml")
    var object = component.createObject(flowLayout, {
      id: id,
      boxWidth: width,
      boxHeight: height,
      boxX: x,
      boxY: y
    });
  }

  ColumnLayout {
    id: mainColumnLayout

    RowLayout {
      id: headerRowLayout

      PlasmaComponents.Label {
        text: "kTile"
        Layout.fillWidth: true
      }
      
      PlasmaComponents.Button {
        icon.name: "list-add-symbolic"
        onClicked: {

          var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

          db.transaction(
            function(tx) {

              var insert = tx.executeSql('INSERT INTO spaces (rowid, width, height, x, y) VALUES((SELECT max(rowid) FROM spaces)+1, 50, 50, 0, 0) RETURNING rowid')

              var component = Qt.createComponent("block.qml")
              var object = component.createObject(flowLayout, {
                id: insert.rows[0].rowid,
                boxWidth: 50,
                boxHeight: 50,
                boxX: 0,
                boxY: 0
              });

              // fix for intially added blocks listing vertically until kwin is reset
              flowLayout.width = parent.parent.parent.width
            }
          )
        }
      }

      PlasmaComponents.Button {
        icon.name: showNumbers ? "password-show-on" : "password-show-off"
        onClicked: {
          showNumbers = !showNumbers
        }
      }
      
      PlasmaComponents.Button {
        icon.name: "dialog-close"
        onClicked: {
          mainDialog.visible = false;
        }
      }
    }

    Rectangle {
      id: spacesContainer
      width: 850
      height: 400
      color: "transparent"

      ScrollView {
        id: scrollview1
        anchors.fill: parent
        anchors.margins: 10
        // anchors.topMargin: 10
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        
        ColumnLayout {
          width: parent.width

          Flow {

            id: flowLayout
            width: parent.parent.width
            spacing: 10

            Component.onCompleted: {
              
              var component = Qt.createComponent("block.qml")

              var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

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
          mainDialog.show();
        }
      }
    );

    for (var i = 1; i <= 12; i++) {
      let count = i
      KWin.registerShortcut(
        "kTile Position " + count,
        "kTile Position " + count,
        "",
        function() {
          tileWindow(workspace.activeClient, count);
        }
      );
    }

    // KWin.registerShortcut(
    //   "kTile Close",
    //   "kTile Close",
    //   "Escape",
    //   function() {
    //     mainDialog.visible = false;
    //   }
    // );

    var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

    db.transaction(
      function(tx) {

        tx.executeSql('CREATE TABLE IF NOT EXISTS spaces(rowid INTEGER PRIMARY KEY AUTOINCREMENT, width INTEGER, height INTEGER, x INTEGER, y INTEGER)');
        tx.executeSql('CREATE TABLE IF NOT EXISTS grid(x INTEGER, y INTEGER, gap INTEGER)');

        var rs = tx.executeSql('SELECT rowid, * FROM grid');

        if(rs.rows.length == 0) {
          tx.executeSql('INSERT INTO grid VALUES(?, ?, ?)', [ 8, 6, 0 ]);
        }
      }
    )
  }
}
