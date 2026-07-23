import QtQuick 2.6
import Sailfish.Silica 1.0
import SpotAmbience 1.0

Page {
    id: page
    objectName: "graphPage"
    property int mode: 1
    property int windowOffset: 0
    property int chartNumberSize: Math.round((Theme.fontSizeExtraLarge * 1.5) / 3.0)
    property var chartData: ({ "points": [], "startMs": 0, "endMs": 0,
                               "rangeText": "", "hasPrevious": false, "hasNext": false })
    property var hourlyData: []

    function updateChart() {
        if (!chart)
            return
        if (mode === 0) {
            chartData = spot.chartHourWindow()
            chart.points = chartData.points
            chart.xMinimum = chartData.startMs
            chart.xMaximum = chartData.endMs
        } else if (mode === 1) {
            chartData = spot.chartWindow(windowOffset)
            chart.points = chartData.points
            chart.xMinimum = chartData.startMs
            chart.xMaximum = chartData.endMs
        } else {
            chart.points = spot.chartPoints(mode === 2 ? "week" : "month")
            chart.xMinimum = 0
            chart.xMaximum = 0
        }
        hourlyData = spot.upcomingHourlyAverages(24)
    }

    function clearChartSelection() {
        if (chart)
            chart.clearSelection()
    }

    function resetView() {
        mode = 1
        windowOffset = 0
        if (rangeSelector)
            rangeSelector.currentIndex = 1
        updateChart()
        clearChartSelection()
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            resetView()
            if (app.navigationInitialized && pageStack.depth === 1 && !pageStack.nextPage(page))
                app.attachMainPage(page)
        }
    }
    Component.onCompleted: updateChart()

    Connections {
        target: spot
        onStateChanged: page.updateChart()
        onConfigChanged: page.updateChart()
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
                text: qsTr("Price graph")
            }

            ComboBox {
                id: rangeSelector
                width: parent.width
                label: qsTr("Range")
                currentIndex: 1
                menu: ContextMenu {
                    MenuItem { text: qsTr("Hour") }
                    MenuItem { text: qsTr("Day") }
                    MenuItem { text: qsTr("Seven days") }
                    MenuItem { text: qsTr("Thirty-one days") }
                }
                onCurrentIndexChanged: {
                    page.mode = currentIndex
                    page.windowOffset = 0
                    page.updateChart()
                    page.clearChartSelection()
                }
            }

            PageNavigationRow {
                visible: page.mode === 1
                height: visible ? Theme.itemSizeSmall : 0
                label: page.chartData.rangeText
                previousEnabled: page.chartData.hasPrevious
                nextEnabled: page.chartData.hasNext
                onPreviousClicked: { page.windowOffset -= 1; page.updateChart(); page.clearChartSelection() }
                onNextClicked: { page.windowOffset += 1; page.updateChart(); page.clearChartSelection() }
            }

            PriceChart {
                id: chart
                width: parent.width
                height: Math.max(260, Math.min(page.width * 0.72, page.height * 0.52))
                cheapThreshold: spot.cheapBelow
                expensiveThreshold: spot.expensiveAbove
                numberFontPixelSize: page.chartNumberSize
            }

            Rectangle {
                id: selectedBreakdown
                property var details: chart.selectedDetails
                property bool hasDetails: details && details.available === true
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                height: hasDetails ? breakdownContent.height + 2 * Theme.paddingMedium : 0
                visible: hasDetails
                radius: Theme.paddingSmall
                color: "#b4000000"

                function price(value) {
                    return Number(value || 0).toFixed(2) + " c/kWh"
                }

                Column {
                    id: breakdownContent
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: Theme.paddingMedium
                    }
                    spacing: Theme.paddingSmall

                    Label {
                        width: parent.width
                        color: "#ffffff"
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Selected hour: %1").arg(selectedBreakdown.details.time)
                    }

                    Repeater {
                        model: [
                            { "label": qsTr("Transfer price"), "value": selectedBreakdown.details.transfer, "color": "#73c68e" },
                            { "label": qsTr("Electricity tax"), "value": selectedBreakdown.details.electricityTax, "color": "#b87cdc" },
                            { "label": qsTr("Spot price"), "value": selectedBreakdown.details.spot, "color": "#469beb" },
                            { "label": qsTr("VAT"), "value": selectedBreakdown.details.vat, "color": "#f5b246" }
                        ]
                        delegate: Row {
                            width: breakdownContent.width
                            spacing: Theme.paddingSmall
                            Rectangle {
                                id: componentMarker
                                width: Theme.paddingMedium
                                height: Theme.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.color
                            }
                            Label {
                                id: componentLabel
                                width: parent.width * 0.60 - componentMarker.width - 2 * parent.spacing
                                color: modelData.color
                                font.pixelSize: Theme.fontSizeExtraSmall
                                text: modelData.label
                            }
                            Label {
                                width: parent.width - componentMarker.width - componentLabel.width
                                       - 2 * parent.spacing
                                color: modelData.color
                                font.pixelSize: Theme.fontSizeExtraSmall
                                horizontalAlignment: Text.AlignRight
                                text: selectedBreakdown.price(modelData.value)
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#4dffffff"
                    }

                    Row {
                        width: parent.width
                        Label {
                            width: parent.width * 0.62
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: Theme.fontSizeSmall
                            text: qsTr("Full price")
                        }
                        Label {
                            width: parent.width * 0.38
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: Theme.fontSizeSmall
                            horizontalAlignment: Text.AlignRight
                            text: selectedBreakdown.price(selectedBreakdown.details.total)
                        }
                    }
                }
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                height: Math.max(implicitHeight, Theme.fontSizeExtraSmall)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: page.mode === 0
                      ? qsTr("Quarter-hour prices for the current hour, the two preceding hours, and the two following hours.")
                      : (page.mode === 1
                         ? ""
                         : (page.mode === 2
                            ? qsTr("Daily average prices for the last seven days.")
                            : qsTr("Daily average prices for the last thirty-one days.")))
            }

            HourlyPriceTable {
                width: parent.width
                title: qsTr("Upcoming hourly averages")
                rows: page.hourlyData
                emptyText: qsTr("No future prices are available")
                reserveInstructionSpace: true
            }
            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}
