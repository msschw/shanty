// SH-602 Phase B: Library page — the navigation root. SH-1004 (UX-15,
// owner direction 2026-11-14): the page is BROWSE-ONLY — the managed
// folders / whitelist / blacklist editing moved to the Settings page
// (whose view model shares the same store); this page re-reads the config
// from the store on every activation (cross-visibility read path) and
// rescans, so changes made in Settings are reflected here. The filtered
// file listing with selection and the "play selection" queue-building
// flow (SH-604 bridge) remain.
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../navigation.js" as Nav

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
        Nav.showPage(navigationFacade, pageStack, "player")
    }

    function pushPlayerPage() {
        Nav.showPage(navigationFacade, pageStack, "player")
    }

    function pushSettingsPage() {
        Nav.showPage(navigationFacade, pageStack, "settings")
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            // SH-1004: re-read the shared config from the store (Settings
            // may have changed it) and refresh the listing. reload() clears
            // the selection (V6) — a fresh activation starts a fresh browse.
            library.reload()
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
                // record); this item is its only entry point. SH-801: the
                // show() command routes it like every other sibling.
                text: qsTr("Playlist")
                onClicked: Nav.showPage(navigationFacade, pageStack, "playlist")
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

            // ---- filtered file listing + selection ---------------------------
            // (SH-1004: the managed folders / whitelist / blacklist editing
            // sections moved to SettingsPage — this page is browse-only.)
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

                    // SH-809 (UX-8): long-press context menu. The catalogue
                    // (which commands are offered per row/state) is
                    // VIEW-MODEL owned and host-tested
                    // (LibraryViewModel::rowCommands); the facade's
                    // rowCommands() is the open-guard and the empty
                    // catalogue (out-of-range row) is the "do not open"
                    // signal. The menu items map 1:1 onto the documented
                    // library-row catalogue ["play", "addToQueue"]; the
                    // selection toggle stays the visible fallback. Rejected
                    // commands surface through the bridges (documented
                    // no-ops → shell toasts).
                    onPressAndHold: {
                        if (library.rowCommands(index).length > 0) {
                            contextMenu.open()
                        }
                    }

                    ContextMenu {
                        id: contextMenu

                        MenuItem {
                            text: qsTr("Play")
                            onClicked: player.playFromPaths([modelData])
                        }
                        MenuItem {
                            text: qsTr("Add to queue")
                            onClicked: player.enqueue(modelData)
                        }
                    }
                }
            }
        }
    }
}
