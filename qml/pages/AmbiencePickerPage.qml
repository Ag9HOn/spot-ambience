import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    property string band
    property string currentUrl
    property string currentName
    property var info: spot.mappingInfo(band)
    signal ambienceSelected(string url, string displayName)

    SilicaListView {
        id: ambienceList
        anchors.fill: parent
        model: spot.availableAmbiences
        header: Column {
            width: page.width
            PageHeader { title: page.info.bandName; description: qsTr("Choose ambience") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                color: spot.ambienceAvailable(page.currentUrl) ? Theme.primaryColor : Theme.highlightColor
                wrapMode: Text.Wrap
                text: page.currentUrl.length > 0
                      ? qsTr("Selected: %1%2").arg(page.currentName).arg(spot.ambienceAvailable(page.currentUrl) ? "" : qsTr(" · unavailable"))
                      : qsTr("No ambience selected")
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                text: qsTr("Preferred default: %1").arg(page.info.preferredName)
            }
            Label {
                visible: page.info.usingFallback
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                color: Theme.secondaryHighlightColor
                wrapMode: Text.Wrap
                text: qsTr("Effective fallback: %1").arg(page.info.effectiveName)
            }
            Item { width: 1; height: Theme.paddingMedium }
        }
        delegate: BackgroundItem {
            width: ListView.view.width
            height: Theme.itemSizeLarge
            onClicked: {
                page.ambienceSelected(modelData.url, modelData.displayName)
                pageStack.pop()
            }
            Image {
                id: preview
                anchors { left: parent.left; leftMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                width: Theme.itemSizeMedium
                height: Theme.itemSizeMedium
                source: modelData.wallpaperUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
            Column {
                anchors { left: preview.right; leftMargin: Theme.paddingMedium; right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                Label { width: parent.width; text: modelData.displayName; truncationMode: TruncationMode.Fade }
                Label {
                    width: parent.width
                    color: Theme.secondaryColor
                    font.pixelSize: Theme.fontSizeExtraSmall
                    text: modelData.url === page.currentUrl ? qsTr("Current mapping") : (modelData.favorite ? qsTr("Favourite") : "")
                    truncationMode: TruncationMode.Fade
                }
            }
        }
        ViewPlaceholder {
            enabled: ambienceList.count === 0
            text: qsTr("No ambiences found")
            hintText: qsTr("Return after the Sailfish ambience database has loaded.")
        }
        VerticalScrollDecorator {}
    }
}
