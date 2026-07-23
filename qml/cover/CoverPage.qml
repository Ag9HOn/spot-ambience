import QtQuick 2.6
import Sailfish.Silica 1.0

CoverBackground {
    id: cover
    allowResize: true

    property color bandColor: {
        if (spot.bandKey === "negative") return "#f2fbff"
        if (spot.bandKey === "cheap") return "#45a9ff"
        if (spot.bandKey === "middle") return "#f5a742"
        if (spot.bandKey === "expensive") return "#ff5e73"
        return Theme.secondaryHighlightColor
    }

    Rectangle {
        anchors.fill: parent
        color: cover.bandColor
        opacity: 0.12
    }

    Rectangle {
        anchors {
            top: parent.top
            bottom: coverActionArea.top
            left: parent.left
        }
        width: Theme.paddingSmall
        color: cover.bandColor
        opacity: 0.9
    }

    Rectangle {
        anchors {
            top: parent.top
            topMargin: Theme.paddingLarge
            left: parent.left
            leftMargin: Theme.paddingLarge
        }
        width: Theme.paddingLarge
        height: width
        radius: width / 2
        color: cover.bandColor
    }

    Column {
        anchors {
            top: parent.top
            topMargin: Theme.paddingLarge
            left: parent.left
            leftMargin: Theme.paddingLarge
            right: parent.right
            rightMargin: Theme.paddingLarge
            bottom: coverActionArea.top
            bottomMargin: Theme.paddingMedium
        }
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignRight
            color: cover.bandColor
            font {
                family: Theme.fontFamilyHeading
                pixelSize: Theme.fontSizeHuge
            }
            fontSizeMode: Text.Fit
            minimumPixelSize: Theme.fontSizeLarge
            text: spot.coverAverageText
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignRight
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
            text: qsTr("c/kWh")
        }

        Item {
            width: parent.width
            height: Math.max(Theme.itemSizeSmall / 2, currentValue.implicitHeight)

            Row {
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                spacing: Theme.paddingSmall

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    source: "image://theme/icon-s-time"
                    opacity: Theme.opacityHigh
                }
                Label {
                    id: currentValue
                    color: cover.bandColor
                    font {
                        family: Theme.fontFamilyHeading
                        pixelSize: Theme.fontSizeMedium
                    }
                    text: spot.coverPriceText
                }
            }
        }
    }

    CoverActionList {
        enabled: true
        CoverAction {
            iconSource: "image://theme/icon-cover-next"
            onTriggered: __silica_applicationwindow_instance.openGraphFromCover()
        }
    }
}
