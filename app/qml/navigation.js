// SH-801 Phase B (UX-1): shared show-and-navigate helper for the Silica
// pages. The view model's show() is the ONE sibling command; the QML side
// mirrors its outcome onto Silica's page stack:
//   - called from the root (pageStack.depth === 1): the push lands the
//     sibling on top (pageStack.push), matching the pre-SH-801 flow;
//   - called from a sibling: show() atomically replaced the view-model top
//     (exactly one navigationChanged fire, never deeper than 2) — the QML
//     side mirrors it with pageStack.replace, so the stack never grows
//     past 2 and the replaced page cannot be navigated back to;
//   - called for the page already on top: early return (the view model
//     would reject the command as "show: already showing <page>"; the UI
//     keeps the current page instead of reloading it).
// Import as:  import "../navigation.js" as Nav   (from the pages/ dir)
// Call as:    Nav.showPage(navigationFacade, pageStack, "player")
.pragma library

function showPage(navigationFacade, pageStack, page) {
    if (navigationFacade.currentPage === page) {
        return
    }
    navigationFacade.show(page)
    if (navigationFacade.currentPage !== page) {
        return  // rejected (unknown page guard) — nothing to sync
    }
    // Qt.resolvedUrl inside this library resolves relative to app/qml/.
    var url = Qt.resolvedUrl("pages/" + page.charAt(0).toUpperCase() +
                             page.slice(1) + "Page.qml")
    if (pageStack.depth === 1) {
        pageStack.push(url)
    } else {
        pageStack.replace(url)
    }
}
