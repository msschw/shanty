// SH-807 (UX-11): minimal yes/no confirmation dialog page. Unlike
// PromptPage (which requires non-empty text to accept), this dialog is
// accept/cancel only — used for the "discard unsaved edits?" guard before
// an import destroys the working playlist.
import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string title
    property string body

    Column {
        width: dialog.width

        DialogHeader {
            title: dialog.title
            acceptText: qsTr("Discard")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: dialog.width - 2 * Theme.horizontalPageMargin
            height: paintedHeight + Theme.paddingLarge
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeMedium
            wrapMode: Text.WordWrap
            text: dialog.body
        }
    }
}
