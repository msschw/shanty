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
import "navigation.js" as Nav

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
        // Confirmations (SH-803 / UX-7): the sparse positive-feedback
        // channel — only queue-building/restoring actions confirm, and the
        // count-bearing strings map by prefix (exact `case` cannot match a
        // varying count).
        var countMatch = /^queue: built from playlist \((\d+)/.exec(reason)
        if (countMatch !== null) {
            var count = parseInt(countMatch[1])
            return count === 1 ? qsTr("Playing playlist (1 entry)")
                               : qsTr("Playing playlist (%1 entries)")
                                     .arg(count)
        }
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
        case "confirmDiscard: not awaiting confirm":
            // SH-807: the discard prompt is only up in awaitingConfirm
            return qsTr("No discard confirmation is pending")
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
        // PlayerViewModel via PlayerFacade (SH-802)
        case "play: playlist is empty":
            return qsTr("The playlist is empty")
        // SH-805 (UX-4): the startup session-restore announcement (fired by
        // the app wiring after the shell exists).
        case "queue: restored":
            return qsTr("Playback restored")
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
        // SH-803 (UX-7): confirmations reuse the same banner — the message
        // arrives as a catalogue string and maps like the rejections do.
        onConfirmed: appWindow.notify(reasonMessage(message))
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

    // -----------------------------------------------------------------------
    // SH-804 (UX-9): the mini-player bar — a slim overlay anchored above the
    // bottom edge, z above the page stack (below the toast). QML-only: every
    // input is an existing facade binding locked by the SH-804 Phase A
    // guards. Visible iff (1) a queue is loaded (player.queueCount > 0),
    // (2) the Player page is NOT current (navigationFacade.currentPage) and
    // (3) no out-of-band page sits on top: pages pushed directly onto the
    // page stack (FilePickerPage / PromptPage — never facade commands) make
    // pageStack.depth EXCEED navigationFacade.depth (the view model's depth
    // never exceeds 2 — locked in tests/navigation). Content: entry title,
    // transport state word and the thin progress line, all riding the
    // snapshot getters the shell's 1 Hz timer keeps fresh. Tap →
    // navigationFacade.show("player") — the SH-801 one-sibling-command: push
    // from the root, atomic replace from a sibling; the bar hides itself
    // through the currentPage binding once the Player is up.
    Rectangle {
        id: playerBar

        z: 500
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.itemSizeSmall
        visible: player.queueCount > 0
                 && navigationFacade.currentPage !== "player"
                 && pageStack.depth === navigationFacade.depth
        color: Qt.rgba(0.1, 0.1, 0.1, 0.9)

        // Thin progress line along the top edge (progressFraction: 0..1).
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            height: Theme.paddingSmall
            width: parent.width * player.progressFraction
            color: Theme.highlightColor
        }

        Label {
            id: playerBarTitle

            anchors.left: parent.left
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 2 * Theme.horizontalPageMargin
                   - playerBarState.implicitWidth - Theme.paddingMedium
            text: player.currentEntry !== ""
                      ? player.currentEntry.split("/").pop()
                      : qsTr("Nothing playing")
            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
            color: Theme.primaryColor
        }

        Label {
            id: playerBarState

            anchors.right: parent.right
            anchors.rightMargin: Theme.horizontalPageMargin
            anchors.verticalCenter: parent.verticalCenter
            text: player.state
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }

        MouseArea {
            anchors.fill: parent
            // SH-801 sync discipline: the shell never calls facade.show()
            // directly — the view-model command alone does NOT move the QML
            // page stack (the pages mirror the outcome via the shared
            // Nav.showPage helper; a bare show() left the VM on "player"
            // while the stack stayed put — found on the device drive). The
            // helper is the single sync point for both directions.
            onClicked: Nav.showPage(navigationFacade, pageStack, "player")
        }
    }
}
