// SH-1008 (UX-19): the per-app settings page the OS Settings app shows
// under Settings → Apps → Shanty (owner direction 2026-11-17). Library
// configuration moved here from the app's Settings page; the in-app page
// keeps the equalizer only.
//
// Mechanism (Phase 0, verified on the emulator 2026-11-18): the Settings
// tree binds app/jolla-settings/shanty.json's "applications/shanty.desktop"
// entry to this file via params.source, then pushes it with the app's
// identity and desktop file. The page runs IN-PROCESS in jolla-settings
// (which is unsandboxed), so the C++ commands come from the app's
// Shanty.Settings QML plugin — the SAME FileIndexStore writes
// ~/.config/shanty/library.conf, and the app re-reads it on its next
// Library activation (browser.rebuild, the SH-1004 cross-visibility path).
//
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import Shanty.Settings 1.0
import com.jolla.settings 1.0

ApplicationSettings {
    id: page

    // ---- local rejection surfacing ---------------------------------------
    // The app shell's toast bridge lives in the app's own engine; this page
    // shows the view model's reason strings in an inline banner instead.
    property string notice

    function showNotice(text) {
        page.notice = text
        noticeTimer.restart()
    }

    Connections {
        target: ShantySettings
        onRejected: page.showNotice(reason)
    }

    Timer {
        id: noticeTimer
        interval: 4000
        onTriggered: page.notice = ""
    }

    // ---- library management (SH-1004 UI, relocated; same idioms as the
    //      app's page had: folder picker for folders, prompt for filter
    //      entries, remove buttons per row) --------------------------------
    function pushFolderPicker() {
        var picker = pageStack.push(Qt.resolvedUrl("FilePickerPage.qml"), {
            mode: "folder",
            startDir: ShantySettings.homeDir(),
            listProvider: ShantySettings.listDirectory
        })
        picker.accepted.connect(function(path) {
            ShantySettings.addFolder(path)
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
            if (section === "whitelist") ShantySettings.addWhitelistEntry(prompt.text.trim())
            else ShantySettings.addBlacklistEntry(prompt.text.trim())
        })
    }

    Rectangle {
        width: page.width
        height: page.notice !== ""
                    ? noticeLabel.implicitHeight + 2 * Theme.paddingMedium
                    : 0
        visible: page.notice !== ""
        color: Qt.rgba(0.1, 0.1, 0.1, 0.9)

        Label {
            id: noticeLabel

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.horizontalPageMargin
            anchors.rightMargin: Theme.horizontalPageMargin
            anchors.verticalCenter: parent.verticalCenter
            text: page.notice
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }
    }

    SectionHeader {
        text: qsTr("Managed folders")
    }

    Repeater {
        model: ShantySettings.libraryFolders

        delegate: ListItem {
            width: page.width
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
                onClicked: ShantySettings.removeFolder(modelData)
            }
        }
    }

    Button {
        x: Theme.horizontalPageMargin
        width: page.width - 2 * Theme.horizontalPageMargin
        text: qsTr("Add folder")
        onClicked: page.pushFolderPicker()
    }

    // ---- whitelist --------------------------------------------------------
    SectionHeader {
        text: qsTr("Whitelist")
    }

    Label {
        x: Theme.horizontalPageMargin
        visible: ShantySettings.libraryWhitelist.length === 0
        text: qsTr("Empty whitelist: every supported file under a managed folder is included")
        width: page.width - 2 * Theme.horizontalPageMargin
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }

    Repeater {
        model: ShantySettings.libraryWhitelist

        delegate: ListItem {
            width: page.width
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
                onClicked: ShantySettings.removeWhitelistEntry(modelData)
            }
        }
    }

    Button {
        x: Theme.horizontalPageMargin
        width: page.width - 2 * Theme.horizontalPageMargin
        text: qsTr("Add whitelist entry")
        onClicked: page.pushFilterPrompt("whitelist")
    }

    // ---- blacklist --------------------------------------------------------
    SectionHeader {
        text: qsTr("Blacklist")
    }

    Repeater {
        model: ShantySettings.libraryBlacklist

        delegate: ListItem {
            width: page.width
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
                onClicked: ShantySettings.removeBlacklistEntry(modelData)
            }
        }
    }

    Button {
        x: Theme.horizontalPageMargin
        width: page.width - 2 * Theme.horizontalPageMargin
        text: qsTr("Add blacklist entry")
        onClicked: page.pushFilterPrompt("blacklist")
    }
}
