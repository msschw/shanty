// SH-603 Phase B: Playlist page — entry list with reorder/remove/add,
// dedupe/clear, and the import/export flow driven by the PlaylistViewModel's
// begin*/confirm*/cancelFlow state machine (file dialogs are the app's own
// FilePickerPage, SH-D603-B1). Reorder uses explicit up/down controls (the
// simplest robust Sailfish flow for a bounded LIFO page list).
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../navigation.js" as Nav

Page {
    id: page

    allowedOrientations: Orientation.All

    // Keep the NavigationViewModel in sync with Silica's page stack: when
    // this sibling page is popped (back swipe), the view model's stack top
    // must unwind as well. SH-801 race guard: the back() call fires only
    // while the view model still shows THIS page on top (see PlayerPage).
    onStatusChanged: {
        if (status === PageStatus.Inactive && pageStack.depth === 1 &&
                navigationFacade.currentPage === "playlist") {
            navigationFacade.back()
        }
    }

    function entryName(entry) {
        return entry.title !== "" ? entry.title
                                  : entry.path.split("/").pop()
    }

    function formatTotal(seconds) {
        if (seconds <= 0) return ""
        var minutes = Math.floor(seconds / 60)
        return qsTr("Total %1 min").arg(minutes)
    }

    // Import flow: Idle/Done/Failed → AwaitingFile, then the dialog hands
    // the picked file to confirmImport() (the view model hops Importing →
    // Done|Failed synchronously and the shell toasts the outcome).
    function pushImportPicker() {
        playlist.beginImport()
        if (playlist.flowState !== "awaitingFile") return
        var picker = pageStack.push(Qt.resolvedUrl("FilePickerPage.qml"), {
            mode: "open",
            nameFilters: [".m3u", ".m3u8"],
            startDir: playlist.homeDir(),
            listProvider: playlist.listDirectory
        })
        picker.accepted.connect(function(path) {
            playlist.confirmImport(path)
        })
        picker.cancelled.connect(function() {
            playlist.cancelFlow()
        })
    }

    function pushExportPicker() {
        playlist.beginExport()
        if (playlist.flowState !== "awaitingFile") return
        var picker = pageStack.push(Qt.resolvedUrl("FilePickerPage.qml"), {
            mode: "save",
            nameFilters: [".m3u", ".m3u8"],
            startDir: playlist.homeDir(),
            listProvider: playlist.listDirectory
        })
        picker.accepted.connect(function(path) {
            playlist.confirmExport(path)
        })
        picker.cancelled.connect(function() {
            playlist.cancelFlow()
        })
    }

    function addLibrarySelection() {
        var paths = library.selectionPaths()
        for (var i = 0; i < paths.length; ++i) playlist.addEntry(paths[i])
    }

    function playEntry(index) {
        // Jump straight into this entry (selection → play, SH-604 bridge).
        // Deliberately NO navigation here (SH-601 row-tap contract, kept in
        // the SH-801 revision): the row tap plays from this entry but keeps
        // the Playlist page on top — the cover shows the now-playing info.
        // Wave-8c device drive caught two traps on the way: the facade API
        // is jumpTo, not jumpToIndex, and the bare name `navigation` is
        // unusable inside pages pushed through pageStack (Silica injects
        // its own int context property with that name).
        var paths = []
        var entries = playlist.entries
        for (var i = 0; i < entries.length; ++i) paths.push(entries[i].path)
        player.loadQueue(paths)
        player.play()
        if (player.queueCount > 0) {
            // loadQueue selects index 0; jumpTo(index) selects AND plays.
            player.jumpTo(index)
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        model: playlist.entries

        // SH-802 (UX-2): on queue-replace play success, surface the Player
        // (sibling switch via the one show() command). The empty-playlist
        // rejection surfaces through the shell's toast instead (no
        // navigation on that path — onPlayStarted fires on success only).
        Connections {
            target: player
            onPlayStarted: Nav.showPage(navigationFacade, pageStack, "player")
        }

        PullDownMenu {
            // SH-802 (UX-3 recorded order): Play first — the explicit
            // "queue this playlist and play it" command; importing never
            // auto-plays (UX-3 decision).
            MenuItem {
                text: qsTr("Play")
                onClicked: playlist.requestPlay()
            }
            MenuItem {
                text: qsTr("Import playlist…")
                onClicked: pushImportPicker()
            }
            MenuItem {
                text: qsTr("Export playlist…")
                onClicked: pushExportPicker()
            }
            MenuItem {
                text: qsTr("Add library selection")
                onClicked: addLibrarySelection()
            }
            MenuItem {
                text: qsTr("Deduplicate")
                onClicked: playlist.deduplicate()
            }
            MenuItem {
                text: qsTr("Clear playlist")
                onClicked: playlist.clear()
            }
        }

        header: Column {
            width: listView.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Playlist")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                //: %1 = entry count, %2 = total duration in minutes
                text: qsTr("%1 entries, %2")
                          .arg(playlist.entryCount)
                          .arg(formatTotal(playlist.totalDurationSeconds))
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }
        }

        delegate: ListItem {
            id: entryItem

            width: listView.width
            contentHeight: Theme.itemSizeSmall

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                       - Theme.iconSizeMedium - 2 * Theme.iconSizeSmall
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: entryName(modelData)
                    truncationMode: TruncationMode.Fade
                }
                Label {
                    width: parent.width
                    text: modelData.path
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                    visible: modelData.title !== ""
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                IconButton {
                    icon.source: "image://theme/icon-m-up"
                    enabled: index > 0
                    onClicked: playlist.moveEntry(index, index - 1)
                }
                IconButton {
                    icon.source: "image://theme/icon-m-down"
                    enabled: index < playlist.entryCount - 1
                    onClicked: playlist.moveEntry(index, index + 1)
                }
                IconButton {
                    icon.source: "image://theme/icon-m-remove"
                    onClicked: playlist.removeAt(index)
                }
            }

            onClicked: playEntry(index)
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: qsTr("Empty playlist")
            hintText: qsTr("Import a playlist or add library tracks")
        }

        VerticalScrollDecorator { }
    }
}
