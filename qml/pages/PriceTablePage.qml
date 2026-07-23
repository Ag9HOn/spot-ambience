import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "priceTablePage"
    property int dayOffset: 0
    property var dayData: ({ "rows": [], "hasPrevious": false,
                             "hasNext": false, "resolvedOffset": 0 })

    function updateDay() {
        dayData = spot.priceDay(dayOffset)
        if (dayData.resolvedOffset !== undefined)
            dayOffset = dayData.resolvedOffset
    }

    onStatusChanged: if (status === PageStatus.Active) {
        dayOffset = 0
        updateDay()
    }
    Component.onCompleted: updateDay()

    Connections {
        target: spot
        onStateChanged: page.updateDay()
        onConfigChanged: page.updateDay()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height

        AppPullDownMenu {}

        Column {
            id: content
            width: page.width
            PageHeader { title: qsTr("Prices") }
            PageNavigationRow {
                label: page.dayData.isToday ? qsTr("Today") : qsTr("Price day")
                previousEnabled: page.dayData.hasPrevious
                nextEnabled: page.dayData.hasNext
                onPreviousClicked: { page.dayOffset -= 1; page.updateDay() }
                onNextClicked: { page.dayOffset += 1; page.updateDay() }
            }
            HourlyPriceTable {
                width: parent.width
                rows: page.dayData.rows
                emptyText: qsTr("No prices for this day")
            }
            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}
