// SH-1003 (UX-14): Phase A stub — no-op rebuild, every mode stays empty so
// the red-first cases fail by assertion. Phase B implements the grouping
// contract (decisions 1–6 in LibraryCatalogue.h).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryCatalogue.h"

namespace shanty {

const char* LibraryCatalogueViewModel::unknownArtistLabel() {
    return "Unknown artist";
}

const char* LibraryCatalogueViewModel::unknownAlbumLabel() {
    return "Unknown album";
}

const char* LibraryCatalogueViewModel::unknownGenreLabel() {
    return "Unknown genre";
}

LibraryCatalogueViewModel::LibraryCatalogueViewModel(ILibraryStore& store,
                                                     LibraryService& library,
                                                     ITagReader& reader)
    : store_(store), library_(library), reader_(reader) {}

void LibraryCatalogueViewModel::rebuild() {}

const std::vector<CatalogueGroup>& LibraryCatalogueViewModel::groups(
    CatalogueMode mode) const {
    static const std::vector<CatalogueGroup> empty;
    (void)mode;
    return empty;
}

const std::vector<std::string>& LibraryCatalogueViewModel::playlistPaths()
    const {
    static const std::vector<std::string> empty;
    return empty;
}

}  // namespace shanty
