// SH-601 Phase B: cover page with basic now-playing info (SH-604 bridge).
// The context properties are visible here like in every page; the shell's
// 1 Hz timer and the controller-state wiring keep the snapshot fresh.
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
