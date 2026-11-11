// SH-602/SH-605 Phase B: minimal text-input dialog page. Used for
// whitelist/blacklist entries (Library), profile creation and renaming
// (Settings). The pushing page reads the entered text from the returned
// page instance's `text` property in its accepted handler.
import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: dialog

    property string title
    property string inputLabel
    property alias text: inputField.text
    property alias placeholderText: inputField.placeholderText

    canAccept: inputField.text.trim() !== ""

    Column {
        width: dialog.width

        DialogHeader {
            title: dialog.title
            acceptText: qsTr("OK")
        }

        TextField {
            id: inputField

            width: dialog.width
            label: dialog.inputLabel
            placeholderText: dialog.inputLabel
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }
    }
}
