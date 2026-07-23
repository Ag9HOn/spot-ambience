import QtQuick 2.6
import Sailfish.Silica 1.0

BackgroundItem {
    id: root
    property var hourData: ({ "time": "", "spot": "", "transfer": "", "price": "", "quarters": [],
                              "current": false, "ambiguous": false, "offset": "" })
    property bool expanded: false
    property int quarterCount: hourData.quarters.length
    property bool complete: quarterCount === 4
    property int summaryHeight: (hourData.ambiguous || !complete)
                                ? Theme.itemSizeMedium : Theme.itemSizeSmall

    height: summaryHeight + (expanded ? hourData.quarters.length * Theme.itemSizeSmall : 0)
    onClicked: expanded = !expanded

    Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.InOutQuad } }

    Rectangle {
        width: parent.width
        height: root.summaryHeight
        color: Theme.highlightBackgroundColor
        opacity: root.hourData.current ? Theme.highlightBackgroundOpacity : 0
    }
    Item {
        width: parent.width
        height: root.summaryHeight
        Icon {
            anchors { left: parent.left; leftMargin: Theme.paddingMedium; verticalCenter: parent.verticalCenter }
            source: root.expanded ? "image://theme/icon-m-up" : "image://theme/icon-m-down"
            color: root.hourData.current ? Theme.highlightColor : Theme.secondaryColor
        }
        Row {
            anchors { left: parent.left; right: parent.right; leftMargin: Theme.horizontalPageMargin; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
            Column {
                width: parent.width * 0.28
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    color: root.hourData.current ? Theme.highlightColor : Theme.primaryColor
                    font.bold: true
                    text: root.hourData.time
                }
                Label {
                    visible: root.hourData.ambiguous || !root.complete
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    text: root.hourData.ambiguous
                          ? root.hourData.offset + (root.complete ? "" : " · " + qsTr("%1/4 prices").arg(root.quarterCount))
                          : qsTr("%1/4 prices").arg(root.quarterCount)
                }
            }
            Label {
                width: parent.width * 0.24
                horizontalAlignment: Text.AlignHCenter
                color: root.hourData.current ? Theme.highlightColor : Theme.primaryColor
                font.bold: root.hourData.current
                text: root.hourData.spot
            }
            Label {
                width: parent.width * 0.24
                horizontalAlignment: Text.AlignHCenter
                color: root.hourData.current ? Theme.highlightColor : Theme.primaryColor
                font.bold: root.hourData.current
                text: root.hourData.transfer
            }
            Label {
                width: parent.width * 0.24
                horizontalAlignment: Text.AlignHCenter
                color: root.hourData.current ? Theme.highlightColor : Theme.primaryColor
                font.bold: root.hourData.current
                text: root.hourData.price
            }
        }
    }
    Column {
        y: root.summaryHeight
        width: parent.width
        visible: root.expanded
        Repeater {
            model: root.hourData.quarters
            delegate: Item {
                width: root.width
                height: Theme.itemSizeSmall
                Rectangle {
                    anchors.fill: parent
                    color: Theme.highlightBackgroundColor
                    opacity: modelData.current ? Theme.highlightBackgroundOpacity : 0
                }
                Rectangle {
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    height: 1
                    anchors { top: parent.top; horizontalCenter: parent.horizontalCenter }
                    color: Theme.secondaryColor
                    opacity: Theme.opacityFaint
                }
                Row {
                    anchors { left: parent.left; right: parent.right; leftMargin: Theme.horizontalPageMargin; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                    Label {
                        width: parent.width * 0.28
                        horizontalAlignment: Text.AlignHCenter
                        color: modelData.current ? Theme.highlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: modelData.current
                        text: modelData.time
                    }
                    Label {
                        width: parent.width * 0.24
                        horizontalAlignment: Text.AlignHCenter
                        color: modelData.current ? Theme.highlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: modelData.current
                        text: modelData.spot
                    }
                    Label {
                        width: parent.width * 0.24
                        horizontalAlignment: Text.AlignHCenter
                        color: modelData.current ? Theme.highlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: modelData.current
                        text: modelData.transfer
                    }
                    Label {
                        width: parent.width * 0.24
                        horizontalAlignment: Text.AlignHCenter
                        color: modelData.current ? Theme.highlightColor : Theme.secondaryColor
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: modelData.current
                        text: modelData.price
                    }
                }
            }
        }
    }
}
