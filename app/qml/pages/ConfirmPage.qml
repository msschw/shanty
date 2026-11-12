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
    /// SH-807: re-emitted so the pushing page can react without touching
    /// the page stack (the acceptDestination idiom owns the navigation —
    /// pushing a page from inside the accepted() handler races the
    /// dialog's pop and desyncs the flow state from the page stack).
    signal confirmed()
    signal dismissed()

    // SH-807: Silica's default accept() navigates FORWARD (navigateForward())
    // which — without an acceptDestination — leaves the dialog ON the stack
    // (on device it reappeared when the picker popped above it), while
    // pushing the picker from inside the accepted() handler races the
    // dialog's pop (the picker never landed; the flow state desynced).
    // Take control: advance the flow via confirmed() and pop the dialog in
    // one step; the pushing page pushes the picker only when this dialog
    // reports PageStatus.Inactive (deterministically out of the stack).
    // The reject path keeps Silica's default (navigateBack pops the
    // dialog — verified on device).
    function accept() {
        confirmed()
        pageStack.pop()
    }

    onRejected: dismissed()

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
