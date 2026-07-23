import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    objectName: "mainPage"

    function attachPriceTable() {
        app.attachPriceTable(page)
    }

    onStatusChanged: attachPriceTable()

    Timer {
        interval: 30000
        repeat: true
        running: Qt.application.active
        onTriggered: spot.reload()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height

        AppPullDownMenu {}

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            Item { width: 1; height: Theme.paddingLarge }

            Label {
                width: parent.width
                height: Theme.itemSizeLarge
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                text: qsTr("Spot Ambience")
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("AVERAGE FOR THE NEXT HOUR")
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingSmall
                Label { color: Theme.primaryColor; font.pixelSize: Theme.fontSizeHuge; text: spot.averageText }
                Label { anchors.baseline: parent.children[0].baseline; color: Theme.secondaryColor; text: qsTr("c/kWh") }
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryHighlightColor
                text: spot.bandText + " · " + spot.effectiveAmbienceName
                wrapMode: Text.Wrap
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
                text: qsTr("Four quarter-hours")
            }
            PriceTableHeader { width: parent.width }
            Repeater {
                model: spot.quarterValues
                delegate: Row {
                    width: content.width - 2 * Theme.horizontalPageMargin
                    x: Theme.horizontalPageMargin
                    Label { width: parent.width * 0.28; horizontalAlignment: Text.AlignHCenter; text: modelData.time }
                    Label { width: parent.width * 0.24; horizontalAlignment: Text.AlignHCenter; text: modelData.spot }
                    Label { width: parent.width * 0.24; horizontalAlignment: Text.AlignHCenter; text: modelData.transfer }
                    Label { width: parent.width * 0.24; horizontalAlignment: Text.AlignHCenter; text: modelData.price }
                }
            }
            Label {
                visible: spot.quarterValues.length === 0
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryColor
                text: qsTr("Waiting for a complete price window")
                wrapMode: Text.Wrap
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeMedium
                text: qsTr("Automation")
            }
            Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("Status: %1").arg(spot.enabled ? qsTr("Enabled") : qsTr("Disabled")) }
            Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("Last refresh: %1").arg(spot.lastRefreshText) }
            Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("Next daily check: %1").arg(spot.nextRefreshText) }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("Cached prices: %1").arg(spot.cacheRangeText)
            }

            Label {
                visible: spot.warningText.length > 0
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                text: spot.warningText
                wrapMode: Text.Wrap
            }
            Label {
                visible: spot.errorText.length > 0
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                text: spot.errorText
                wrapMode: Text.Wrap
            }

            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}
