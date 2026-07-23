import QtQuick 2.6
import Sailfish.Silica 1.0

Column {
    id: table
    property var rows: []
    property string title: ""
    property string emptyText: qsTr("No prices are available")
    property bool reserveInstructionSpace: false

    width: parent ? parent.width : 0

    Label {
        visible: table.title.length > 0
        width: parent.width
        height: visible ? implicitHeight : 0
        horizontalAlignment: Text.AlignHCenter
        color: Theme.highlightColor
        font.pixelSize: Theme.fontSizeMedium
        text: table.title
    }
    Item {
        width: parent.width
        height: table.reserveInstructionSpace ? Theme.fontSizeExtraSmall : 0
    }
    PriceTableHeader { width: parent.width }
    Repeater {
        model: table.rows
        delegate: Column {
            width: table.width
            property bool startsDate: index === 0
                                      || table.rows[index - 1].dateKey !== modelData.dateKey

            SectionHeader {
                visible: parent.startsDate
                width: parent.width
                height: visible ? implicitHeight : 0
                horizontalAlignment: Text.AlignHCenter
                text: modelData.dateText
            }
            HourlyPriceRow {
                width: parent.width
                hourData: modelData
            }
        }
    }
    Label {
        visible: table.rows.length === 0
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        color: Theme.secondaryColor
        text: table.emptyText
    }
}
