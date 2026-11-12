// SH-604 Phase B: Player page — now-playing metadata, transport controls,
// seek bar and the error panel. Disabled states come from the view model's
// derived flags (transportEnabled gate, canGoNext/canGoPrevious queue
// edges); position/progress refresh through the shell's 1 Hz timer plus the
// controller-state wiring, per the PlayerViewModel snapshot contract.
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../navigation.js" as Nav

Page {
    id: page

    allowedOrientations: Orientation.All

    // Keep the NavigationViewModel in sync with Silica's page stack: when
    // this sibling page is popped (back swipe), the view model's stack top
    // must unwind as well. SH-801 race guard: the back() call fires only
    // while the view model still shows THIS page on top — during an atomic
    // show() replace the view model already names the target page, so a
    // deactivating replaced page can never pop the wrong sibling.
    onStatusChanged: {
        if (status === PageStatus.Inactive && pageStack.depth === 1 &&
                navigationFacade.currentPage === "player") {
            navigationFacade.back()
        }
    }

    function trackName(entry) {
        return entry.split("/").pop()
    }

    function formatTime(seconds) {
        if (seconds < 0) return "--:--"
        var minutes = Math.floor(seconds / 60)
        var rest = seconds - minutes * 60
        return minutes + ":" + (rest < 10 ? "0" : "") + rest
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        // SH-801 (UX-1): the Player page's pulley — every view reachable
        // from here. "Library" unwinds to the root (the page pop back-syncs
        // back() exactly like a back-swipe); "Playlist"/"Settings" are
        // sibling switches through the one show() command (atomic replace).
        // Lives INSIDE the SilicaFlickable (Silica requires it — a
        // PullDownMenu as a direct Page child never attaches).
        PullDownMenu {
            MenuItem {
                text: qsTr("Playlist")
                onClicked: Nav.showPage(navigationFacade, pageStack, "playlist")
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: Nav.showPage(navigationFacade, pageStack, "settings")
            }
            MenuItem {
                text: qsTr("Library")
                onClicked: pageStack.pop()
            }
        }

        Column {
            id: column

            width: page.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Now playing")
            }

            // ---- metadata ----------------------------------------------------
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: player.currentEntry !== ""
                          ? trackName(player.currentEntry)
                          : qsTr("Nothing playing")
                font.pixelSize: Theme.fontSizeLarge
                truncationMode: TruncationMode.Fade
                color: player.currentEntry !== "" ? Theme.highlightColor
                                                  : Theme.secondaryColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                //: %1 = current index, %2 = queue length, %3 = queue state
                text: qsTr("Track %1 of %2 (%3)")
                          .arg(player.queueCount > 0
                                   ? player.currentIndex + 1 : 0)
                          .arg(player.queueCount)
                          .arg(player.state)
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                truncationMode: TruncationMode.Fade
            }

            // ---- seek bar ------------------------------------------------------
            Slider {
                id: seekBar

                width: parent.width
                minimumValue: 0.0
                maximumValue: 1.0
                stepSize: 0.0
                value: player.progressFraction
                valueText: formatTime(Math.round(seekBar.value *
                                                 Math.max(player.durationSeconds, 0)))
                // Enabled only with a known duration and live transport
                // (the seek command is rejected without a duration anyway).
                enabled: player.transportEnabled && player.durationSeconds > 0
                onReleased: {
                    if (player.durationSeconds > 0) {
                        player.seekTo(Math.round(seekBar.value *
                                                 player.durationSeconds))
                    }
                    // The drag assignment broke the snapshot binding; the
                    // seek bar follows the view model's refreshed snapshot.
                    value = Qt.binding(function() {
                        return player.progressFraction
                    })
                }
            }

            // ---- transport -----------------------------------------------------
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingLarge

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "image://theme/icon-m-previous"
                    enabled: player.canGoPrevious
                    onClicked: player.previous()
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: player.state === "playing"
                                     ? "image://theme/icon-m-pause"
                                     : "image://theme/icon-m-play"
                    enabled: player.transportEnabled
                    onClicked: player.togglePlayPause()
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "image://theme/icon-m-next"
                    enabled: player.canGoNext
                    onClicked: player.next()
                }
            }

            // ---- error panel ------------------------------------------------------
            Column {
                visible: player.hasError
                width: parent.width
                spacing: Theme.paddingMedium

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    text: qsTr("Playback error: %1")
                              .arg(player.lastErrorMessage !== ""
                                       ? player.lastErrorMessage
                                       : player.lastErrorPath)
                    color: Theme.errorColor
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                }

                Button {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    // Documented recovery: fresh-attempt play().
                    text: qsTr("Try again")
                    enabled: !player.transportEnabled
                    onClicked: player.retryPlayback()
                }
            }
        }
    }
}
