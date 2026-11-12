// SH-605 Phase B: Settings page — equalizer band sliders bound to the
// current profile (persisted per band through the facade, SH-D605-B1),
// profile list with select/rename/delete, AutoEq import (name prompt +
// file picker) and apply/reset. Rejections surface through the shell's
// toast mapping of the view model's reason catalogue.
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    // Keep the NavigationViewModel in sync with Silica's page stack: when
    // this sibling page is popped (back swipe), the view model's stack top
    // must unwind as well (back() is only valid when a sibling sits on the
    // root — the guard prevents double-pops).
    onStatusChanged: {
        if (status === PageStatus.Inactive && pageStack.depth === 1 &&
                navigationFacade.depth > 1) {
            navigationFacade.back()
        }
    }

    // The current profile item out of the facade's profile list.
    function currentProfile() {
        var all = settings.profiles
        for (var i = 0; i < all.length; ++i) {
            if (all[i].isCurrent) return all[i]
        }
        return null
    }

    function pushCreatePrompt() {
        var prompt = pageStack.push(Qt.resolvedUrl("PromptPage.qml"), {
            title: qsTr("New profile"),
            inputLabel: qsTr("Profile name"),
            placeholderText: qsTr("Profile name")
        })
        prompt.accepted.connect(function() {
            // A fresh profile is a flat, immediately editable start
            // (auto-selected by the view model).
            settings.createProfile(prompt.text.trim(),
                                   10)  // 10-band default grid
        })
    }

    function pushRenamePrompt(oldName) {
        var prompt = pageStack.push(Qt.resolvedUrl("PromptPage.qml"), {
            title: qsTr("Rename profile"),
            inputLabel: qsTr("Profile name"),
            placeholderText: oldName
        })
        prompt.accepted.connect(function() {
            settings.renameProfile(oldName, prompt.text.trim())
        })
    }

    // AutoEq import: prompt for the display name, then pick the profile
    // file; the facade reads the file and hands name + data + path verbatim
    // to the view model.
    function pushImportFlow() {
        var prompt = pageStack.push(Qt.resolvedUrl("PromptPage.qml"), {
            title: qsTr("Import AutoEq profile"),
            inputLabel: qsTr("Profile name"),
            placeholderText: qsTr("Profile name")
        })
        prompt.accepted.connect(function() {
            var name = prompt.text.trim()
            var picker = pageStack.push(Qt.resolvedUrl("FilePickerPage.qml"), {
                mode: "open",
                nameFilters: [],
                startDir: settings.homeDir(),
                listProvider: settings.listDirectory
            })
            picker.accepted.connect(function(path) {
                settings.importAutoEqProfile(name, path)
            })
        })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
            }

            // ---- equalizer ------------------------------------------------------
            SectionHeader {
                text: qsTr("Equalizer")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                //: %1 = current profile name
                text: currentProfile() !== null
                          ? qsTr("Current profile: %1")
                                .arg(currentProfile().name)
                          : qsTr("No profile selected")
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }

            // Band sliders bound to the current profile's gains (release
            // persists the band through the facade, SH-D605-B1).
            Repeater {
                model: currentProfile() !== null ? currentProfile().gains : []

                delegate: Row {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin

                    Label {
                        //: %1 = 1-based band number
                        text: qsTr("Band %1").arg(index + 1)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        width: Theme.itemSizeSmall
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Slider {
                        width: parent.width - Theme.itemSizeSmall
                        minimumValue: -12.0
                        maximumValue: 12.0
                        stepSize: 0.5
                        value: modelData
                        valueText: value.toFixed(1) + " dB"
                        onReleased: {
                            settings.setBandGain(index, value)
                            // If the write was rejected the model did not
                            // change: re-follow the stored snapshot.
                            value = Qt.binding(function() { return modelData })
                        }
                    }
                }
            }

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingLarge

                Button {
                    text: qsTr("Apply")
                    onClicked: settings.applyCurrentProfile()
                }
                Button {
                    text: qsTr("Reset")
                    onClicked: settings.resetEqualizer()
                }
            }

            // ---- profiles ---------------------------------------------------------
            SectionHeader {
                text: qsTr("Profiles")
            }

            Repeater {
                model: settings.profiles

                delegate: ListItem {
                    id: profileItem

                    width: column.width
                    contentHeight: Theme.itemSizeSmall

                    Column {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter

                        Label {
                            width: parent.width
                            text: modelData.name +
                                  (modelData.source === "imported"
                                       ? qsTr(" (imported)") : "")
                            color: modelData.isCurrent
                                       ? Theme.highlightColor
                                       : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                        }
                        Label {
                            width: parent.width
                            //: %1 = band count
                            text: qsTr("%1 bands").arg(modelData.bandCount)
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                        }
                    }

                    onClicked: settings.selectProfile(modelData.name)
                    onPressAndHold: contextMenu.open()

                    ContextMenu {
                        id: contextMenu

                        MenuItem {
                            text: qsTr("Rename")
                            onClicked: pushRenamePrompt(modelData.name)
                        }
                        MenuItem {
                            text: qsTr("Delete")
                            onClicked: settings.deleteProfile(modelData.name)
                        }
                    }
                }
            }

            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingLarge

                Button {
                    text: qsTr("New profile")
                    onClicked: pushCreatePrompt()
                }
                Button {
                    text: qsTr("Import AutoEq…")
                    onClicked: pushImportFlow()
                }
            }
        }
    }
}
