// SH-601 Phase B / SH-801: application shell. The Silica ApplicationWindow
// hosts the stable Library root page and the cover; the page stack is driven
// through the NavigationFacade (navigationFacade), whose view model owns the
// LIFO state machine — sibling pages are shown only via the facade's ONE
// show(page) command (root push / atomic sibling replace, mirrored onto
// pageStack by navigation.js) and pop only through navigationFacade.back()
// (synced from Silica's page pop sibling pages).
import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow

    initialPage: Component { LibraryPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // SH-801 (UX-1): player-first landing. The Library root is stable and
    // always the initialPage; when the landing decision says "player" (a
    // queue exists at startup — session queue, or a restored one once queue
    // persistence lands), the Player page is pushed over the fresh root at
    // startup: stack [Library, Player], recorded through the facade.
    Component.onCompleted: {
        if (navigationFacade.landing === "player") {
            navigationFacade.show("player")
            if (navigationFacade.depth > 1) {
                pageStack.push(Qt.resolvedUrl("pages/PlayerPage.qml"))
            }
        }
    }

    // SH-604 decision: PlayerViewModel exposes a snapshot that only refresh()
    // re-reads — the shell's 1 Hz timer drives position/progress updates for
    // the Player page and the cover (state changes refresh through the app
    // layer's controller wiring in app/shanty.cpp).
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: player.refresh()
    }

    // -----------------------------------------------------------------------
    // Notifier banner ("toast"): every view-model rejection/reason comes
    // back through a facade signal; the shell surfaces them here.
    function notify(message) {
        toastLabel.text = message
        toast.opacity = 1.0
        toastTimer.restart()
    }

    // Map the documented view-model reason catalogue strings (kept verbatim
    // on the C++ side) to translated text; unknown strings are shown as-is.
    function reasonMessage(reason) {
        switch (reason) {
        // NavigationViewModel (SH-801 revision: show() is the one sibling
        // command; the open* strings are gone)
        case "show: already showing player":
        case "show: already showing settings":
        case "show: already showing playlist":
            return qsTr("Already on that page")
        case "show: library is the root":
        case "back: already at the root":
            return qsTr("Already on the library page")
        // LibraryViewModel (SH-602)
        case "addFolder: empty path":
            return qsTr("Cannot add an empty folder path")
        case "addFolder: already managed":
            return qsTr("That folder is already managed")
        case "addWhitelistEntry: empty path":
            return qsTr("Cannot add an empty whitelist entry")
        case "addBlacklistEntry: empty path":
            return qsTr("Cannot add an empty blacklist entry")
        case "setSelected: index out of range":
            return qsTr("That selection is no longer valid")
        // PlaylistViewModel (SH-603)
        case "beginImport: flow already open":
        case "beginExport: flow already open":
            return qsTr("A playlist file dialog is already open")
        case "confirmImport: not awaiting file":
        case "confirmExport: not awaiting file":
            return qsTr("No playlist file is being awaited")
        case "confirmImport: export pending":
        case "confirmExport: import pending":
            return qsTr("A different playlist operation is still pending")
        case "cancel: no open flow":
            return qsTr("No open playlist operation to cancel")
        case "cancel: flow in progress":
            return qsTr("The playlist operation is already in progress")
        // PlaybackController via PlayerFacade (SH-604)
        case "play: empty queue":
        case "next: empty queue":
        case "previous: empty queue":
        case "jumpTo: empty queue":
            return qsTr("The queue is empty")
        case "next: end of queue":
            return qsTr("End of the queue")
        case "previous: start of queue":
            return qsTr("Start of the queue")
        case "jumpTo: index out of range":
            return qsTr("That queue position no longer exists")
        case "pause: not playing":
            return qsTr("Nothing is playing")
        case "resume: not playing":
            return qsTr("Nothing is paused")
        case "stop: nothing playing":
            return qsTr("Nothing is playing")
        // SettingsViewModel (SH-605)
        case "create: empty profile name":
        case "rename: empty profile name":
        case "import: empty profile name":
            return qsTr("Please enter a profile name")
        case "create: invalid band count":
            return qsTr("Invalid equalizer band count")
        case "create: profile name already exists":
        case "rename: name already exists":
            return qsTr("A profile with that name already exists")
        case "create: rejected by store":
        case "rename: rejected by store":
            return qsTr("The profile could not be saved")
        case "rename: unknown profile":
        case "delete: unknown profile":
        case "select: unknown profile":
        case "apply: unknown profile":
            return qsTr("Unknown profile")
        case "apply: no profile selected":
            return qsTr("No equalizer profile is selected")
        case "apply: invalid profile":
            return qsTr("The selected profile is invalid")
        case "apply: rejected by effect chain":
            return qsTr("The equalizer rejected the profile")
        // SettingsFacade band-edit reasons (SH-D605-B1)
        case "band: no profile selected":
            return qsTr("No equalizer profile is selected")
        case "band: invalid band index":
            return qsTr("That equalizer band does not exist")
        case "band: rejected by store":
            return qsTr("The profile could not be saved")
        }
        return reason
    }

    Connections {
        target: navigationFacade
        onInvalidNavigation: appWindow.notify(reasonMessage(reason))
    }
    Connections {
        target: library
        onInvalidReason: appWindow.notify(reasonMessage(reason))
    }
    Connections {
        target: playlist
        onInvalidCommand: appWindow.notify(reasonMessage(reason))
        onFlowChanged: {
            if (playlist.flowState === "done") {
                appWindow.notify(qsTr("Playlist operation completed"))
            } else if (playlist.flowState === "failed" &&
                       playlist.lastError !== "") {
                appWindow.notify(playlist.lastError)
            }
        }
    }
    Connections {
        target: player
        onInvalidCommand: appWindow.notify(reasonMessage(reason))
        onSeekRejected: appWindow.notify(qsTr("Seeking is not available yet"))
    }
    Connections {
        target: settings
        onRejected: appWindow.notify(reasonMessage(reason))
        onImported: appWindow.notify(qsTr("AutoEq profile imported"))
        onApplied: appWindow.notify(qsTr("Equalizer profile applied"))
        onResetDone: appWindow.notify(qsTr("Equalizer reset to flat"))
        onImportRejected: {
            if (messages.length > 0) appWindow.notify(messages[0])
        }
    }

    Rectangle {
        id: toast

        z: 999
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.paddingLarge
        width: Math.min(parent.width - 2 * Theme.horizontalPageMargin,
                        toastLabel.implicitWidth + 2 * Theme.paddingMedium)
        height: toastLabel.implicitHeight + 2 * Theme.paddingMedium
        radius: height / 2
        // Theme.highlightDimmer is undefined on the Qt 5.6-era Silica of
        // the 5.1 emulator image — a translucent dark overlay reads the
        // same and binds without the runtime warning.
        color: Qt.rgba(0.1, 0.1, 0.1, 0.9)
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Label {
            id: toastLabel

            anchors.centerIn: parent
            width: Math.min(implicitWidth,
                            appWindow.width - 2 * Theme.horizontalPageMargin)
            truncationMode: TruncationMode.Fade
            color: Theme.primaryColor
        }
    }

    Timer {
        id: toastTimer

        interval: 3000
        onTriggered: toast.opacity = 0
    }
}
