import QtQuick 2.6
import Sailfish.Silica 1.0

PullDownMenu {
    MenuItem { text: qsTr("About"); onClicked: app.openAbout() }
    MenuItem { text: qsTr("Settings"); onClicked: app.openSettings() }
    MenuItem { text: qsTr("Refresh now"); onClicked: spot.requestRefresh() }
}
