import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kwin 2.0
import QtQuick.LocalStorage 2.15

// API Docs: https://develop.kde.org/docs/plasma/kwin/api/

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
  property variant screenList: getScreens()

  function getScreens() {
    var tmpList = ['Auto Display']

    for (var i = 0; i < workspace.numScreens; i++) {
      tmpList.push("Display " + (i + 1))
    }
    return tmpList
  }

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
        const rs = tx.executeSql('SELECT rowid, * FROM spaces2 WHERE rowid = ' + pos);

        let newWidth = ((rs.rows[0].width / 100) * (screen.width - gap)) - gap
        let newHeight = ((rs.rows[0].height / 100) * (screen.height - gap)) - gap
        let newX = ((rs.rows[0].x / 100) * (screen.width - gap)) + gap + screen.x
        let newY = ((rs.rows[0].y / 100) * (screen.height - gap)) + gap + screen.y
        let getDisplay = rs.rows[0].display

        window.setMaximize(false, false);
        window.geometry = Qt.rect(newX, newY, newWidth, newHeight);

        if(getDisplay !== 0) {
          workspace.sendClientToScreen(window, getDisplay - 1)
        }

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
        id: add
        icon.name: "list-add-symbolic"

        SequentialAnimation {
          id: addAnim
          loops: Animation.Infinite
          alwaysRunToEnd: true

          // expand the button
          PropertyAnimation {
            target: add
            property: "scale"
            to: 1.2
            duration: 350
            easing.type: Easing.InOutQuad
          }

          // shrink back to normal
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

              var insert = tx.executeSql('INSERT INTO spaces2 (rowid, width, height, x, y, display) VALUES((SELECT max(rowid) FROM spaces2)+1, 50, 50, 0, 0, 0) RETURNING rowid')

              var component = Qt.createComponent("block.qml")
              var object = component.createObject(flowLayout, {
                id: insert.rows[0].rowid,
                boxWidth: 50,
                boxHeight: 50,
                boxX: 0,
                boxY: 0,
                displayNum: 0,
              });

              addAnim.stop()

              // fix for intially added blocks listing vertically until kwin is reset
              flowLayout.width = parent.parent.parent.width
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
