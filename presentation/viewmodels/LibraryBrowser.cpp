// SH-1005 (UX-16): Phase A stub — the browser state layer returns only
// empty snapshots and fires no events so the red-first cases fail by
// assertion (never a crash). Phase B implements decisions B1–B7.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryBrowser.h"

#include <cstddef>
#include <functional>

namespace shanty {

LibraryBrowserViewModel::LibraryBrowserViewModel(
    LibraryCatalogueViewModel& catalogue)
    : catalogue_(catalogue) {}

void LibraryBrowserViewModel::setMode(BrowserMode mode) { (void)mode; }

BrowserMode LibraryBrowserViewModel::mode() const {
    return BrowserMode::Artists;
}

void LibraryBrowserViewModel::rebuild() {}

const std::vector<LibraryBrowserViewModel::Row>&
LibraryBrowserViewModel::rows() const {
    static const std::vector<Row> empty;
    return empty;
}

void LibraryBrowserViewModel::openEntry(std::size_t index) {
    (void)index;
}

bool LibraryBrowserViewModel::canGoBack() const { return false; }

void LibraryBrowserViewModel::back() {}

std::vector<LibraryViewModel::RowCommand>
LibraryBrowserViewModel::rowCommands(std::size_t index) const {
    (void)index;
    return {};
}

}  // namespace shanty
