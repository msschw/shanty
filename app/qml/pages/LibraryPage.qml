// SH-1005 Phase B (UX-16, owner direction 2026-11-14): the Library page
// IS the media browser. The SH-602 track list with selection ("play
// selection") is REPLACED by the SH-1003 catalogue surfaced through the
// SH-1005 browser facade ("browser"): a view-mode selector (Artists /
// Albums / Genre / Playlists — the owner's minimum set), category rows
// with drill-down (Artists two levels: artist → albums → tracks; Albums /
// Genre one level; Playlists flat over SH-1002's discovered .m3u paths)
// and an in-page back affordance (drill-down never leaves the page — no
// new navigation contract; SH-801's landing is untouched).
//
// Row semantics (decisions recorded in UX_TRACE.md, SH-1005 Phase B):
//   - Category row tap     → drill one level down (browser.openEntry).
//   - Track row tap        → play immediately, WITHOUT leaving the page
//                            (the SH-601 "row taps play the entry without
//                            leaving the page" contract; the player toast
//                            bar carries the state; SH-1006 controls).
//   - Playlist row tap     → resolve + queue + play through the SH-802
//                            bridge (PlaylistFacade::requestPlaylistFile
//                            → playRequested → PlayerFacade::playFromPaths);
//                            NO auto-navigation to the Playlist editor —
//                            the Playlist page stays the editing surface.
//   - pressAndHold menus   → SH-809 re-catalogue (browser.rowCommands is
//                            the open-guard; every level is uniform in row
//                            kind, so the two static menus map 1:1 onto
//                            the host-tested command catalogues).
//   - Empty state          → one-line hint (no special-casing in the data
//                            layer, per SH-1003's "no special-casing
//                            strings").
//
// Activation re-scans through the shared pipeline (browser.rebuild — one
// store.load + both scanner queries + one tag read per track), preserving
// SH-1004's cross-visibility with the Settings-side config edits.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../navigation.js" as Nav

Page {
    id: page

    allowedOrientations: Orientation.All

    function pushPlayerPage() {
        Nav.showPage(navigationFacade, pageStack, "player")
    }

    function pushSettingsPage() {
        Nav.showPage(navigationFacade, pageStack, "settings")
    }

    function pushPlaylistPage() {
        Nav.showPage(navigationFacade, pageStack, "playlist")
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            // Re-read the shared config from the store (Settings may have
            // changed it) and rescan; B7: the drill-down resets on rebuild.
            browser.rebuild()
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
                text: qsTr("Playlist")
                onClicked: pushPlaylistPage()
            }
            MenuItem {
                text: qsTr("Rescan library")
                onClicked: browser.rebuild()
            }
        }

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Library")
            }

            // ---- in-page back affordance (B5) ---------------------------------
            // Drill-down back never touches the page stack; the affordance
            // appears only when a level is behind the current one. Read as
            // the PROPERTY (no parens): canGoBack is a rowsChanged-notified
            // Q_PROPERTY, not an invokable (a call in a binding throws a
            // TypeError and leaves the item at its default visibility —
            // found on the device drive).
            IconButton {
                x: Theme.horizontalPageMargin
                visible: browser.canGoBack
                icon.source: "image://theme/icon-m-back"
                onClicked: browser.back()
            }

            // ---- view-mode selector (B1; owner minimum set) --------------------
            Flow {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingLarge

                // The already-active mode is disabled (dimmed) — visible
                // current-mode feedback without styling tricks on the
                // Qt 5.6-era Silica.
                Button {
                    text: qsTr("Artists")
                    enabled: browser.mode !== "artists"
                    onClicked: browser.mode = "artists"
                }
                Button {
                    text: qsTr("Albums")
                    enabled: browser.mode !== "albums"
                    onClicked: browser.mode = "albums"
                }
                Button {
                    text: qsTr("Genre")
                    enabled: browser.mode !== "genres"
                    onClicked: browser.mode = "genres"
                }
                Button {
                    text: qsTr("Playlists")
                    enabled: browser.mode !== "playlists"
                    onClicked: browser.mode = "playlists"
                }
            }

            // ---- empty-state hint (one line per empty surface) -----------------
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: browser.rows.length === 0
                text: browser.mode === "playlists"
                          ? qsTr("No playlists found")
                          : qsTr("No media found — add folders in Settings")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            // ---- browsable rows (B3 snapshot; re-queried on rowsChanged) -------
            Repeater {
                model: browser.rows

                delegate: ListItem {
                    width: column.width
                    contentHeight: Theme.itemSizeSmall

                    onClicked: {
                        if (modelData.kind === "category") {
                            browser.openEntry(index)
                        } else if (modelData.kind === "playlist") {
                            // SH-802 bridge: resolve + queue + play; the
                            // rejections toast (service errors verbatim,
                            // empty playlist player-side). No navigation.
                            playlist.requestPlaylistFile(modelData.path)
                        } else {
                            // Track row: play in place (no page change).
                            player.playFromPaths([modelData.path])
                        }
                    }

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        truncationMode: TruncationMode.Fade
                        color: modelData.kind === "category"
                                   ? Theme.highlightColor
                                   : Theme.primaryColor
                    }

                    // SH-809 (UX-8): long-press context menus, re-catalogued
                    // for the browser rows (B6). The open-guard is the
                    // facade's rowCommands() (empty = do not open); every
                    // LEVEL is uniform in row kind, so each kind owns its
                    // static menu and only the matching one is ever opened —
                    // the menu items map 1:1 onto the host-tested catalogues
                    // (track rows ["play", "addToQueue"], playlist rows
                    // ["play"]).
                    onPressAndHold: {
                        if (browser.rowCommands(index).length > 0) {
                            if (modelData.kind === "playlist") {
                                playlistMenu.open()
                            } else {
                                trackMenu.open()
                            }
                        }
                    }

                    ContextMenu {
                        id: trackMenu

                        MenuItem {
                            text: qsTr("Play")
                            onClicked: player.playFromPaths([modelData.path])
                        }
                        MenuItem {
                            text: qsTr("Add to queue")
                            onClicked: player.enqueue(modelData.path)
                        }
                    }

                    ContextMenu {
                        id: playlistMenu

                        MenuItem {
                            text: qsTr("Play")
                            onClicked: playlist.requestPlaylistFile(
                                           modelData.path)
                        }
                    }
                }
            }
        }
    }
}
