// SH-602 Phase B: Library page — the navigation root. Folder management
// (add via a folder picker page, remove), whitelist/blacklist editing
// (add via the prompt dialog, remove), the filtered file listing with
// selection and the "play selection" queue-building flow (SH-604 bridge).
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    // Re-run the filtered listing after an effective config mutation; scan()
    // also resets the selection (view-model contract V7), which is fine —
    // the listing itself changed.
    function rescan() {
        library.scan()
    }

    function playSelection() {
        var paths = library.selectionPaths()
        if (paths.length === 0) {
            // Surface the documented controller reason ("play: empty queue")
            // through the shell's toast instead of a bespoke message.
            player.play()
            return
        }
        // SH-602 → SH-604 bridge: build the queue, then start playback.
        player.loadQueue(paths)
        player.play()
        navigationFacade.openPlayer()
        if (navigationFacade.depth > 1) {
            pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
        }
    }

    function pushFolderPicker() {
        var picker = pageStack.push(Qt.resolvedUrl("FilePickerPage.qml"), {
            mode: "folder",
            startDir: library.homeDir(),
            listProvider: library.listDirectory
        })
        picker.accepted.connect(function(path) {
            library.addFolder(path)
            rescan()
        })
    }

    function pushFilterPrompt(section) {
        var prompt = pageStack.push(Qt.resolvedUrl("PromptPage.qml"), {
            title: section === "whitelist" ? qsTr("Add whitelist entry")
                                           : qsTr("Add blacklist entry"),
            inputLabel: qsTr("Path"),
            placeholderText: qsTr("Path")
        })
        prompt.accepted.connect(function() {
            if (section === "whitelist") library.addWhitelistEntry(prompt.text.trim())
            else library.addBlacklistEntry(prompt.text.trim())
        })
    }

    function pushPlayerPage() {
        navigationFacade.openPlayer()
        if (navigationFacade.depth > 1) {
            pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
        }
    }

    function pushSettingsPage() {
        navigationFacade.openSettings()
        if (navigationFacade.depth > 1) {
            pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active && library.tracks.length === 0) {
            rescan()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        PullDownMenu {
            MenuItem {
                text: qsTr("Settings")
                onClicked: pushSettingsPage()
            }
            MenuItem {
                text: qsTr("Player")
                onClicked: pushPlayerPage()
            }
            MenuItem {
                // Wave-8 follow-up: the SH-603 playlist page previously had
                // no navigation path (see NavigationViewModel.h decision
                // record); this item is its only entry point.
                text: qsTr("Playlist")
                onClicked: {
                    navigationFacade.openPlaylist()
                    pageStack.push(Qt.resolvedUrl("PlaylistPage.qml"))
                }
            }
            MenuItem {
                text: qsTr("Rescan library")
                onClicked: rescan()
            }
        }

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Library")
            }

            // ---- managed folders -------------------------------------------
            SectionHeader {
                text: qsTr("Managed folders")
            }

            Repeater {
                model: library.folders

                delegate: ListItem {
                    width: column.width
                    contentHeight: Theme.itemSizeSmall

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                               - Theme.iconSizeMedium
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        truncationMode: TruncationMode.Fade
                    }
                    IconButton {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: "image://theme/icon-m-remove"
                        onClicked: {
                            library.removeFolder(modelData)
                            rescan()
                        }
                    }
                }
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Add folder")
                onClicked: pushFolderPicker()
            }

            // ---- whitelist --------------------------------------------------
            SectionHeader {
                text: qsTr("Whitelist")
            }

            Label {
                x: Theme.horizontalPageMargin
                visible: library.whitelist.length === 0
                text: qsTr("Empty whitelist: every supported file under a managed folder is included")
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            Repeater {
                model: library.whitelist

                delegate: ListItem {
                    width: column.width
                    contentHeight: Theme.itemSizeSmall

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                               - Theme.iconSizeMedium
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        truncationMode: TruncationMode.Fade
                    }
                    IconButton {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: "image://theme/icon-m-remove"
                        onClicked: {
                            library.removeWhitelistEntry(modelData)
                            rescan()
                        }
                    }
                }
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Add whitelist entry")
                onClicked: pushFilterPrompt("whitelist")
            }

            // ---- blacklist --------------------------------------------------
            SectionHeader {
                text: qsTr("Blacklist")
            }

            Repeater {
                model: library.blacklist

                delegate: ListItem {
                    width: column.width
                    contentHeight: Theme.itemSizeSmall

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                               - Theme.iconSizeMedium
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        truncationMode: TruncationMode.Fade
                    }
                    IconButton {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: "image://theme/icon-m-remove"
                        onClicked: {
                            library.removeBlacklistEntry(modelData)
                            rescan()
                        }
                    }
                }
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Add blacklist entry")
                onClicked: pushFilterPrompt("blacklist")
            }

            // ---- filtered file listing + selection ---------------------------
            SectionHeader {
                //: %1 = number of listed tracks
                text: qsTr("Tracks (%1)").arg(library.tracks.length)
            }

            // Wave-8b device-drive fix: a plain Row overflows the 720-px
            // screen — the third button ("Play selection") was clipped off
            // and unreachable by touch (checklist A2 blocker). Flow wraps
            // the buttons onto the next line instead.
            Flow {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingLarge

                Button {
                    text: qsTr("Select all")
                    onClicked: library.selectAll()
                }
                Button {
                    text: qsTr("Clear selection")
                    onClicked: library.clearSelection()
                }
                Button {
                    text: qsTr("Play selection")
                    onClicked: playSelection()
                }
            }

            Repeater {
                model: library.tracks

                delegate: ListItem {
                    width: column.width
                    contentHeight: Theme.itemSizeSmall

                    function isSelected() {
                        return library.selection.indexOf(index) >= 0
                    }

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                               - Theme.iconSizeMedium
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        truncationMode: TruncationMode.Fade
                        color: isSelected() ? Theme.highlightColor
                                            : Theme.primaryColor
                    }
                    IconButton {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        icon.source: isSelected()
                                         ? "image://theme/icon-m-acknowledge"
                                         : "image://theme/icon-m-dot"
                        onClicked: {
                            library.setSelected(index, !isSelected())
                        }
                    }
                }
            }
        }
    }
}
