import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    id: page
    property var bands: ["unknown", "negative", "cheap", "middle", "expensive"]
    property bool draftEnabled: false
    property bool draftTestMode: false
    property bool draftIncludeVat: false
    property string draftVatPercent: "25.5"
    property bool draftPersonalPriceEnabled: false
    property bool draftPersonalPriceDayNight: false
    property string draftPersonalPriceFlat: "0"
    property string draftPersonalPriceDay: "0"
    property string draftPersonalPriceNight: "0"
    property string draftElectricityTax: "2.91788"
    property string draftCheapBelow: "5"
    property string draftExpensiveAbove: "15"
    property string draftPublicationTime: "14:15"
    property bool draftOverrideEnabled: false
    property string draftOverrideStart: "00:00"
    property string draftOverrideEnd: "08:00"
    property var draftOverrideMapping: ({ "url": "", "displayName": "Inari Blue" })
    property var draftMappings: ({})
    property string validationMessage: ""

    RemorsePopup { id: clearRemorse }
    Component { id: timePickerComponent; TimePickerDialog {} }

    function loadDraft() {
        var settings = spot.settingsSnapshot()
        draftEnabled = settings.enabled
        draftTestMode = settings.testMode
        draftIncludeVat = settings.includeVat
        draftVatPercent = settings.vatPercent.toString()
        draftPersonalPriceEnabled = settings.personalPriceEnabled
        draftPersonalPriceDayNight = settings.personalPriceDayNight
        draftPersonalPriceFlat = settings.personalPriceFlat.toString()
        draftPersonalPriceDay = settings.personalPriceDay.toString()
        draftPersonalPriceNight = settings.personalPriceNight.toString()
        draftElectricityTax = settings.electricityTax.toString()
        draftCheapBelow = settings.cheapBelow.toString()
        draftExpensiveAbove = settings.expensiveAbove.toString()
        draftPublicationTime = settings.publicationTime
        draftOverrideEnabled = settings.overrideEnabled
        draftOverrideStart = settings.overrideStart
        draftOverrideEnd = settings.overrideEnd
        draftOverrideMapping = settings.overrideMapping
        draftMappings = settings.mappings
        validationMessage = ""
    }

    function chooseTime(currentValue, callback) {
        var parts = currentValue.split(":")
        var dialog = pageStack.push(timePickerComponent, {
            "hour": Number(parts[0]),
            "minute": Number(parts[1])
        })
        dialog.accepted.connect(function() {
            var hour = dialog.hour < 10 ? "0" + dialog.hour : dialog.hour.toString()
            var minute = dialog.minute < 10 ? "0" + dialog.minute : dialog.minute.toString()
            callback(hour + ":" + minute)
        })
    }

    function draftMapping(band) {
        return band === "override" ? draftOverrideMapping : (draftMappings[band] || {})
    }

    function setDraftMapping(band, url, displayName) {
        var mapping = { "url": url, "displayName": displayName }
        if (band === "override") {
            draftOverrideMapping = mapping
            return
        }
        var copy = {}
        for (var i = 0; i < bands.length; ++i) {
            var key = bands[i]
            copy[key] = key === band ? mapping : draftMappings[key]
        }
        draftMappings = copy
    }

    function chooseAmbience(band) {
        var mapping = draftMapping(band)
        app.openAmbiencePicker(band, mapping.url || "", mapping.displayName || "",
                               function(url, displayName) {
            page.setDraftMapping(band, url, displayName)
        })
    }

    function save() {
        var vat = Number(draftVatPercent.replace(",", "."))
        var flatRate = Number(draftPersonalPriceFlat.replace(",", "."))
        var dayRate = Number(draftPersonalPriceDay.replace(",", "."))
        var nightRate = Number(draftPersonalPriceNight.replace(",", "."))
        var electricityTax = Number(draftElectricityTax.replace(",", "."))
        var cheap = Number(draftCheapBelow.replace(",", "."))
        var expensive = Number(draftExpensiveAbove.replace(",", "."))
        if (isNaN(vat) || vat < 0 || vat > 100) {
            validationMessage = qsTr("VAT rate must be between 0 and 100 percent")
            return
        }
        if (isNaN(cheap) || cheap <= 0 || isNaN(expensive) || expensive <= cheap) {
            validationMessage = qsTr("The expensive threshold must be greater than the positive cheap threshold")
            return
        }
        var selectedRate = draftPersonalPriceDayNight ? Math.max(dayRate, nightRate) : flatRate
        if (draftPersonalPriceEnabled && (isNaN(selectedRate) || selectedRate < 0 || selectedRate > 100
                                           || isNaN(electricityTax) || electricityTax < 0 || electricityTax > 100)) {
            validationMessage = qsTr("Transfer price must be between 0 and 100 c/kWh")
            return
        }
        var saved = spot.applySettings({
            "enabled": draftEnabled,
            "testMode": draftTestMode,
            "includeVat": draftIncludeVat,
            "vatPercent": vat,
            "personalPriceEnabled": draftPersonalPriceEnabled,
            "personalPriceDayNight": draftPersonalPriceDayNight,
            "personalPriceFlat": flatRate,
            "personalPriceDay": dayRate,
            "personalPriceNight": nightRate,
            "electricityTax": electricityTax,
            "cheapBelow": cheap,
            "expensiveAbove": expensive,
            "publicationTime": draftPublicationTime,
            "overrideEnabled": draftOverrideEnabled,
            "overrideStart": draftOverrideStart,
            "overrideEnd": draftOverrideEnd,
            "overrideMapping": draftOverrideMapping,
            "mappings": draftMappings
        })
        if (saved)
            pageStack.pop()
        else
            validationMessage = spot.errorText
    }

    Component.onCompleted: loadDraft()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height

        PullDownMenu {
            MenuItem { text: qsTr("About"); onClicked: app.openAbout() }
            MenuItem {
                text: qsTr("Clear cached prices")
                onClicked: clearRemorse.execute(qsTr("Clearing cached prices"), function() { spot.clearCache() })
            }
            MenuItem { text: qsTr("Refresh now"); onClicked: spot.requestRefresh() }
        }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingSmall
            Item { width: 1; height: Theme.paddingLarge }
            Label {
                width: parent.width
                height: Theme.itemSizeLarge
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                text: qsTr("Settings")
            }

            TextSwitch {
                text: qsTr("Automatic ambience changes")
                description: qsTr("The background timer changes the ambience only after these settings are saved.")
                checked: page.draftEnabled
                onClicked: page.draftEnabled = checked
            }
            TextSwitch {
                enabled: page.draftEnabled
                text: qsTr("Test price cycle")
                description: qsTr("Use synthetic prices and rotate through all five ambience mappings every 15 minutes.")
                checked: page.draftTestMode
                onClicked: page.draftTestMode = checked
            }

            SectionHeader {
                text: qsTr("Prices")
                horizontalAlignment: Text.AlignHCenter
            }
            TextSwitch {
                text: qsTr("Include Finnish VAT")
                description: qsTr("Thresholds and displayed prices use the selected VAT rate.")
                checked: page.draftIncludeVat
                onClicked: page.draftIncludeVat = checked
            }
            TextField {
                width: parent.width
                enabled: page.draftIncludeVat
                label: qsTr("Vat rate (%)")
                text: page.draftVatPercent
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftVatPercent = text
                EnterKey.onClicked: focus = false
            }
            TextSwitch {
                text: qsTr("Add transfer price")
                description: qsTr("Transfer price and electricity tax are added after VAT to displayed prices and ambience thresholds.")
                checked: page.draftPersonalPriceEnabled
                onClicked: page.draftPersonalPriceEnabled = checked
            }
            ComboBox {
                width: parent.width
                enabled: page.draftPersonalPriceEnabled
                label: qsTr("Transfer pricing")
                currentIndex: page.draftPersonalPriceDayNight ? 1 : 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("One price") }
                    MenuItem { text: qsTr("Day and night prices") }
                }
                onCurrentIndexChanged: page.draftPersonalPriceDayNight = currentIndex === 1
            }
            TextField {
                visible: !page.draftPersonalPriceDayNight
                height: visible ? implicitHeight : 0
                width: parent.width
                enabled: page.draftPersonalPriceEnabled
                label: qsTr("Transfer price (c/kWh)")
                text: page.draftPersonalPriceFlat
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftPersonalPriceFlat = text
                EnterKey.onClicked: focus = false
            }
            TextField {
                visible: page.draftPersonalPriceDayNight
                height: visible ? implicitHeight : 0
                width: parent.width
                enabled: page.draftPersonalPriceEnabled
                label: qsTr("Day transfer price (07:00 – 22:00) (c/kWh)")
                text: page.draftPersonalPriceDay
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftPersonalPriceDay = text
                EnterKey.onClicked: focus = false
            }
            TextField {
                visible: page.draftPersonalPriceDayNight
                height: visible ? implicitHeight : 0
                width: parent.width
                enabled: page.draftPersonalPriceEnabled
                label: qsTr("Night transfer price (22:00 – 07:00) (c/kWh)")
                text: page.draftPersonalPriceNight
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftPersonalPriceNight = text
                EnterKey.onClicked: focus = false
            }
            TextField {
                width: parent.width
                enabled: page.draftPersonalPriceEnabled
                label: qsTr("Electricity tax (c/kWh)")
                text: page.draftElectricityTax
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftElectricityTax = text
                EnterKey.onClicked: focus = false
            }
            SectionHeader {
                text: qsTr("Ambience thresholds")
                horizontalAlignment: Text.AlignHCenter
            }
            TextField {
                width: parent.width
                label: qsTr("Cheap below (c/kWh)")
                text: page.draftCheapBelow
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftCheapBelow = text
                EnterKey.onClicked: focus = false
            }
            TextField {
                width: parent.width
                label: qsTr("Expensive above (c/kWh)")
                text: page.draftExpensiveAbove
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextChanged: if (activeFocus) page.draftExpensiveAbove = text
                EnterKey.onClicked: focus = false
            }
            ValueButton {
                label: qsTr("Daily publication check")
                value: page.draftPublicationTime
                description: qsTr("Time selection")
                onClicked: page.chooseTime(page.draftPublicationTime, function(value) {
                    page.draftPublicationTime = value
                })
            }

            SectionHeader {
                text: qsTr("Ambience mappings")
                horizontalAlignment: Text.AlignHCenter
            }
            Repeater {
                model: page.bands
                delegate: BackgroundItem {
                    id: mappingDelegate
                    property var mapping: page.draftMapping(modelData)
                    property var info: spot.mappingInfo(modelData)
                    width: parent.width
                    height: Theme.itemSizeLarge
                    enabled: page.draftEnabled
                    onClicked: page.chooseAmbience(modelData)
                    Column {
                        anchors { left: parent.left; right: parent.right; margins: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                        Label { width: parent.width; color: Theme.highlightColor; text: mappingDelegate.info.bandName }
                        Label {
                            width: parent.width
                            color: spot.ambienceAvailable(mappingDelegate.mapping.url || "") ? Theme.primaryColor : Theme.secondaryColor
                            text: mappingDelegate.mapping.url
                                  ? (mappingDelegate.mapping.displayName || mappingDelegate.info.preferredName)
                                    + (spot.ambienceAvailable(mappingDelegate.mapping.url) ? "" : " · " + qsTr("missing"))
                                  : qsTr("Not configured")
                            truncationMode: TruncationMode.Fade
                        }
                    }
                }
            }

            SectionHeader {
                text: qsTr("Timed ambience override")
                horizontalAlignment: Text.AlignHCenter
            }
            TextSwitch {
                enabled: page.draftEnabled
                text: qsTr("Use a fixed ambience during quiet hours")
                description: qsTr("The timed ambience takes priority over price-based mappings.")
                checked: page.draftOverrideEnabled
                onClicked: page.draftOverrideEnabled = checked
            }
            ValueButton {
                enabled: page.draftEnabled && page.draftOverrideEnabled
                label: qsTr("Override starts")
                value: page.draftOverrideStart
                onClicked: page.chooseTime(page.draftOverrideStart, function(value) {
                    page.draftOverrideStart = value
                })
            }
            ValueButton {
                enabled: page.draftEnabled && page.draftOverrideEnabled
                label: qsTr("Override ends")
                value: page.draftOverrideEnd
                onClicked: page.chooseTime(page.draftOverrideEnd, function(value) {
                    page.draftOverrideEnd = value
                })
            }
            ValueButton {
                enabled: page.draftEnabled && page.draftOverrideEnabled
                label: qsTr("Override ambience")
                value: page.draftOverrideMapping.displayName || qsTr("Not configured")
                description: qsTr("Default: Inari Blue")
                onClicked: page.chooseAmbience("override")
            }

            Label {
                visible: page.validationMessage.length > 0
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.highlightColor
                wrapMode: Text.Wrap
                text: page.validationMessage
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.Wrap
                text: qsTr("Saving with automatic changes enabled authorizes the background worker to change the system ambience.")
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Save settings")
                onClicked: page.save()
            }
            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator {}
    }
}
