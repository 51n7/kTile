import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Controls
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import "components" as Components
import org.kde.kwin // defines "Workspace"
import QtQuick.LocalStorage

/*
API Docs: https://develop.kde.org/docs/plasma/kwin/api/
          https://doc.qt.io/qt-6/qtqml-javascript-dynamicobjectcreation.html
*/

PlasmaCore.Dialog {
  id: mainDialog

  location: PlasmaCore.Types.Floating
  flags: Qt.X11BypassWindowManagerHint | Qt.FramelessWindowHint
  visible: false

  property bool editMode: false
  property bool showNumbers: false
  property double gap: 10
  property double spacesCount: 0
  property string database: "ktile"
  property double lastPressTime: 0
  property double doublePressDelay: 500

  function show() {
    var screen = Workspace.clientArea(KWin.FullScreenArea, Workspace.activeScreen, Workspace.currentDesktop);
    mainDialog.visible = true;
    mainDialog.x = screen.x + screen.width/2 - mainDialog.width/2;
    mainDialog.y = screen.y + screen.height/2 - mainDialog.height/2;

    // mainDialog.width = screen.width/2;
    // mainDialog.height = screen.height/2;
  }

  function tileWindow(window, pos) {

    if (!window.normalWindow) return;
    var screen = Workspace.clientArea(KWin.FullScreenArea, Workspace.activeScreen, Workspace.currentDesktop);
    let db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

    db.transaction(
      function(tx) {
        const rs = tx.executeSql('SELECT rowid, * FROM spaces2 WHERE rowid = ' + pos);

        let newWidth = ((rs.rows[0].width / 100) * (screen.width - gap)) - gap
        let newHeight = ((rs.rows[0].height / 100) * (screen.height - gap)) - gap
        let newX = ((rs.rows[0].x / 100) * (screen.width - gap)) + gap + screen.x
        let newY = ((rs.rows[0].y / 100) * (screen.height - gap)) + gap + screen.y
        let getDisplay = rs.rows[0].display

        window.setMaximize(false, false);
        window.frameGeometry = Qt.rect(newX, newY, newWidth, newHeight);

        // if(getDisplay !== 0) {
        //   workspace.sendClientToScreen(window, getDisplay - 1)
        // }

        // experimenting with double tab to move window to second display
        // var currentTime = new Date().getTime();
        // if (currentTime - lastPressTime < doublePressDelay) {
        //   // double press action
        //   workspace.slotWindowToNextScreen()
        // } else {
        //   // single press action
        // }
        // lastPressTime = currentTime;
      }
    )
  }

  function reDraw(id, width, height, x, y, display) {
    var component = Qt.createComponent("components/Block.qml");
    var object = component.createObject(flowLayout, {
      id: id,
      boxWidth: width,
      boxHeight: height,
      boxX: x,
      boxY: y
    });
  }

  Item {
    id: shortcuts

    ShortcutHandler {
      id: mainShortcut
      name: "kTile"
      text: "kTile"
      sequence: "Ctrl+."
      onActivated: {
        if (mainDialog.visible) {
          mainDialog.visible = false;
        } else {
          mainDialog.show();
        }
      }
    }

    Repeater {
      model: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
      delegate: Item {
        ShortcutHandler {
          name: "kTile Position " + (index + 1)
          text: "kTile Position " + (index + 1)
          sequence: ""
          onActivated: {
            tileWindow(Workspace.activeWindow, (index + 1));
          }
        }
      }
    }
  }

  Rectangle {
    width: 850
    height: 400
    color: "transparent"

    ColumnLayout {
      id: mainColumnLayout
      // visible: false
      width: parent.width
      height: parent.height

      RowLayout {
        id: headerRowLayout

        PlasmaComponents.Label {
          text: "kTile"
          Layout.fillWidth: true
        }
        
        PlasmaComponents.Button {
          id: add
          icon.name: "list-add-symbolic"

          SequentialAnimation {
            id: addAnim
            loops: Animation.Infinite
            alwaysRunToEnd: true

            // animate: expand the button
            PropertyAnimation {
              target: add
              property: "scale"
              to: 1.2
              duration: 350
              easing.type: Easing.InOutQuad
            }

            // animate: shrink back to normal
            PropertyAnimation {
              target: add
              property: "scale"
              to: 1.0
              duration: 350
              easing.type: Easing.InOutQuad
            }
          }
          onClicked: {

            var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

            db.transaction(
              function(tx) {

                var insert = tx.executeSql('INSERT INTO spaces2 (rowid, width, height, x, y, display) VALUES((SELECT max(rowid) FROM spaces2)+1, 50, 50, 0, 0, 0) RETURNING rowid');

                var component = Qt.createComponent("components/Block.qml");
                var object = component.createObject(flowLayout, {
                  id: insert.rows[0].rowid,
                  boxWidth: 50,
                  boxHeight: 50,
                  boxX: 0,
                  boxY: 0,
                  displayNum: 0,
                });

                addAnim.stop();

                // fix for intially added blocks listing vertically until kwin is reset
                flowLayout.width = parent.parent.parent.width;
              }
            )
          }
          Component.onCompleted: {
            if(spacesCount) {
              addAnim.stop()
            } else {
              addAnim.start()
            }
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
        width: parent.width
        height: parent.height
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
              width: parent.width
              spacing: 10

              Component.onCompleted: {
              
                var component = Qt.createComponent("components/Block.qml");

                var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

                db.transaction(
                  function(tx) {
                    var rs = tx.executeSql('SELECT rowid, * FROM spaces2');
                    for (var i = 0; i < rs.rows.length; i++) {
                      var object = component.createObject(flowLayout, {
                        id: rs.rows.item(i).rowid,
                        boxWidth: rs.rows.item(i).width,
                        boxHeight: rs.rows.item(i).height,
                        boxX: rs.rows.item(i).x,
                        boxY: rs.rows.item(i).y,
                        displayNum: rs.rows.item(i).display
                      });
                    }

                    spacesCount = rs.rows.length
                  }
                )
              }
            }
          }
        }
      }
    }
  }
  
  Component.onCompleted: {
    print('onCompleted - ktile')

    var db = LocalStorage.openDatabaseSync(database, "1.0", "", 1000000);

    db.transaction(
      function(tx) {

        tx.executeSql('CREATE TABLE IF NOT EXISTS spaces2(rowid INTEGER PRIMARY KEY AUTOINCREMENT, width INTEGER, height INTEGER, x INTEGER, y INTEGER, display INTEGER)');
        tx.executeSql('CREATE TABLE IF NOT EXISTS grid(x INTEGER, y INTEGER, gap INTEGER)');

        var rs = tx.executeSql('SELECT rowid, * FROM grid');

        if(rs.rows.length == 0) {
          tx.executeSql('INSERT INTO grid VALUES(?, ?, ?)', [ 8, 6, 0 ]);
        }
      }
    )
  }
}
