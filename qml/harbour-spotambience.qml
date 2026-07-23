import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Ambience 1.0
import "pages"

ApplicationWindow {
    id: app
    property bool navigationInitialized: false

    function openSettings() {
        pageStack.push(settingsPageComponent)
    }

    function openAbout() {
        pageStack.push(aboutPageComponent)
    }

    function openGraphFromCover() {
        var graph = pageStack.find(function(candidate) {
            return candidate.objectName === "graphPage"
        })
        if (graph)
            pageStack.pop(graph, PageStackAction.Immediate)
        else
            pageStack.push(graphPageComponent, {}, PageStackAction.Immediate)
        app.activate()
    }

    function openAmbiencePicker(bandName, currentUrl, currentName, callback) {
        var picker = pageStack.push(ambiencePickerComponent, {
            "band": bandName,
            "currentUrl": currentUrl,
            "currentName": currentName
        })
        if (callback)
            picker.ambienceSelected.connect(callback)
    }

    function refreshAmbienceInventory() {
        // An empty result is authoritative only after AmbienceModel has
        // completed its asynchronous query. While it is still querying, keep
        // the cached worker fallback visible rather than clearing the picker.
        if (ambienceModel.status !== AmbienceModel.Finished)
            return
        var entries = []
        for (var i = 0; i < ambienceModel.count; ++i) {
            var entry = ambienceModel.get(i)
            if (!entry || !entry.url)
                continue
            entries.push({
                "url": entry.url.toString(),
                "displayName": entry.displayName || entry.fileName || "",
                "wallpaperUrl": entry.wallpaperUrl ? entry.wallpaperUrl.toString() : "",
                "favorite": entry.favorite || false,
                "readOnly": entry.readOnly || false,
                "highlightColor": entry.highlightColor ? entry.highlightColor.toString() : "",
                "primaryColor": entry.primaryColor ? entry.primaryColor.toString() : "",
                "available": true
            })
        }
        // On some sandboxed system versions AmbienceModel reports Finished
        // with zero entries after its private cache is denied. Do not turn a
        // usable public/file-backed inventory into an empty picker in that
        // case. A genuinely empty device still remains empty.
        if (entries.length > 0 || spot.availableAmbiences.length === 0)
            spot.setAvailableAmbiences(entries)
    }

    function attachMainPage(fromPage) {
        if (fromPage.status === PageStatus.Active && !pageStack.nextPage(fromPage))
            pageStack.pushAttached(mainPageComponent)
    }

    function attachPriceTable(fromPage) {
        if (fromPage.status === PageStatus.Active && !pageStack.nextPage(fromPage))
            pageStack.pushAttached(priceTablePageComponent)
    }

    Component { id: graphPageComponent; GraphPage {} }
    Component { id: mainPageComponent; MainPage {} }
    Component { id: priceTablePageComponent; PriceTablePage {} }
    Component { id: settingsPageComponent; SettingsPage {} }
    Component { id: aboutPageComponent; AboutPage {} }
    Component { id: ambiencePickerComponent; AmbiencePickerPage {} }

    initialPage: graphPageComponent
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: Orientation.All

    AmbienceModel {
        id: ambienceModel
        onStatusChanged: if (status === AmbienceModel.Finished) app.refreshAmbienceInventory()
        onCountChanged: if (status === AmbienceModel.Finished) app.refreshAmbienceInventory()
    }

    Timer {
        interval: 700
        running: true
        repeat: false
        onTriggered: {
            pageStack.completeAnimation()
            if (startupPage === "graph") {
                app.navigationInitialized = true
                return
            }
            if (startupPage === "settings") {
                app.openSettings()
                app.navigationInitialized = true
                return
            }
            var target = startupPage === "table" ? priceTablePageComponent : mainPageComponent
            pageStack.push(target, {}, PageStackAction.Immediate)
            app.navigationInitialized = true
        }
    }

    Timer {
        interval: 1000
        running: ambienceModel.status !== AmbienceModel.Finished
                 && spot.availableAmbiences.length === 0
        repeat: true
        onTriggered: spot.reloadAmbiences()
    }

    onApplicationActiveChanged: if (applicationActive) {
        spot.reload()
        refreshAmbienceInventory()
    }
}
