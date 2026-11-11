// SH-603 Phase B: reusable Silica file browser page (SH-D603-B1, see
// PlaylistFacade.h for the decision record). Three modes:
//   "folder" — accept the current directory (library folder adding),
//   "open"   — pick an existing file by tapping it (playlist import,
//              AutoEq import),
//   "save"   — enter a file name in the current directory (playlist export).
// The page is facade-agnostic: the pushing page supplies startDir,
// nameFilters and the listDirectory() invokable of its facade.
import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    property string mode: "open"
    /// Suffix filters (e.g. [".m3u", ".m3u8"], case-insensitive);
    /// an empty list admits all files.
    property var nameFilters: []
    property string startDir
    /// A Q_INVOKABLE reference like playlist.listDirectory.
    property var listProvider: null

    property string currentDir
    property bool _completed: false

    signal accepted(string path)
    signal cancelled()

    function parentOf(dir) {
        if (dir === "/" || dir === "") return "/"
        var parent = dir.substring(0, dir.lastIndexOf("/"))
        return parent === "" ? "/" : parent
    }

    function joinPath(dir, name) {
        return dir === "/" ? "/" + name : dir + "/" + name
    }

    function filterAccepts(name) {
        if (nameFilters.length === 0) return true
        var lower = name.toLowerCase()
        for (var i = 0; i < nameFilters.length; ++i) {
            if (lower.lastIndexOf(nameFilters[i].toLowerCase()) ===
                    lower.length - nameFilters[i].length &&
                lower.length >= nameFilters[i].length) return true
        }
        return false
    }

    function reload() {
        entriesModel.clear()
        if (currentDir === "") currentDir = startDir
        if (listProvider === null) return
        var listing = listProvider(currentDir, nameFilters)
        for (var i = 0; i < listing.length; ++i) entriesModel.append(listing[i])
    }

    function acceptCurrent() {
        if (mode === "folder") {
            _completed = true
            accepted(currentDir)
        } else if (mode === "save" && fileNameField.text.trim() !== "") {
            _completed = true
            accepted(joinPath(currentDir, fileNameField.text.trim()))
        }
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            reload()
        } else if (status === PageStatus.Inactive && !_completed) {
            // Dismissed (back swipe) without a choice.
            cancelled()
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        model: entriesModel

        PullDownMenu {
            MenuItem {
                visible: page.mode !== "open"
                text: page.mode === "folder" ? qsTr("Add this folder")
                                             : qsTr("Save here")
                onClicked: page.acceptCurrent()
            }
            MenuItem {
                text: qsTr("Parent folder")
                onClicked: {
                    page.currentDir = page.parentOf(page.currentDir)
                    page.reload()
                }
            }
        }

        header: Column {
            width: listView.width
            spacing: Theme.paddingSmall

            PageHeader {
                title: page.mode === "folder" ? qsTr("Choose folder")
                                              : (page.mode === "save"
                                                     ? qsTr("Choose export target")
                                                     : qsTr("Choose file"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: page.currentDir
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                truncationMode: TruncationMode.Fade
            }

            TextField {
                id: fileNameField

                visible: page.mode === "save"
                width: parent.width
                placeholderText: qsTr("File name")
                label: page.currentDir
                EnterKey.onClicked: page.acceptCurrent()
            }
        }

        delegate: BackgroundItem {
            width: listView.width

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                text: model.name + (model.isDir ? "/" : "")
                color: model.isDir || page.filterAccepts(model.name)
                           ? Theme.primaryColor
                           : Theme.secondaryColor
                truncationMode: TruncationMode.Fade
            }

            onClicked: {
                if (model.isDir) {
                    page.currentDir = model.path
                    page.reload()
                } else if (page.mode === "open" &&
                           page.filterAccepts(model.name)) {
                    page._completed = true
                    page.accepted(model.path)
                    pageStack.pop()
                }
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: qsTr("No entries")
        }

        VerticalScrollDecorator { }
    }

    ListModel { id: entriesModel }
}
