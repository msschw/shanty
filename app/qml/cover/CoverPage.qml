// SH-601 Phase B: cover page with basic now-playing info (SH-604 bridge).
// The context properties are visible here like in every page; the shell's
// 1 Hz timer and the controller-state wiring keep the snapshot fresh.
// SH-810 (UX-5): a thin position line under the metadata — fed by the same
// 1 Hz refresh (progressFraction's documented clamps: 0.0 without a known
// duration, 1.0 at/beyond the end); no new state, pure QML over the
// already-tested facade properties.
import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    function trackName(entry) {
        return entry.split("/").pop()
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: player.currentEntry !== ""
                      ? trackName(player.currentEntry)
                      : qsTr("Nothing playing")
            font.pixelSize: Theme.fontSizeMedium
            truncationMode: TruncationMode.Fade
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: player.currentEntry !== ""
                      ? player.state
                      : qsTr("Library")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }

        // ---- SH-810 (UX-5): thin position line --------------------------------
        // Two-hair progress track with a highlight fill; width follows the
        // facade's progressFraction (documented clamps cover the no-duration
        // and end-of-track cases), so the line ticks with the shell's 1 Hz
        // cover refresh. Hidden without a current entry — nothing to show.
        Item {
            width: parent.width
            height: Theme.paddingSmall
            visible: player.currentEntry !== ""

            Rectangle {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.paddingMedium
                height: 2
                color: Theme.secondaryColor
                opacity: 0.4

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height
                    width: parent.width * player.progressFraction
                    color: Theme.highlightColor
                }
            }
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-previous"
            onTriggered: player.previous()
        }
        CoverAction {
            iconSource: player.state === "playing"
                            ? "image://theme/icon-cover-pause"
                            : "image://theme/icon-cover-play"
            onTriggered: player.togglePlayPause()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-next"
            onTriggered: player.next()
        }
    }
}
