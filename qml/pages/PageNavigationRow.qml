import QtQuick 2.6
import Sailfish.Silica 1.0

Row {
    id: root
    property string label: ""
    property bool previousEnabled: false
    property bool nextEnabled: false
    signal previousClicked()
    signal nextClicked()

    width: parent ? parent.width : 0
    height: Theme.itemSizeSmall

    IconButton {
        width: Theme.itemSizeLarge
        icon.source: "image://theme/icon-m-left"
        enabled: root.previousEnabled
        onClicked: root.previousClicked()
    }
    Label {
        width: parent.width - 2 * Theme.itemSizeLarge
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignHCenter
        color: Theme.highlightColor
        text: root.label
        truncationMode: TruncationMode.Fade
    }
    IconButton {
        width: Theme.itemSizeLarge
        icon.source: "image://theme/icon-m-right"
        enabled: root.nextEnabled
        onClicked: root.nextClicked()
    }
}
