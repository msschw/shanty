// SH-1005 (UX-16): Phase B — the browser state layer implementing
// decisions B1–B7 (LibraryBrowser.h), locked by tests/library_catalogue.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryBrowser.h"

#include <cstddef>
#include <functional>

namespace shanty {

LibraryBrowserViewModel::LibraryBrowserViewModel(
    LibraryCatalogueViewModel& catalogue, IArtworkReader* artwork)
    : catalogue_(catalogue), artwork_(artwork) {}

void LibraryBrowserViewModel::setMode(BrowserMode mode) {
    // B1: switching the mode resets the drill-down to the mode root.
    mode_ = mode;
    resetNavigation();
}

BrowserMode LibraryBrowserViewModel::mode() const { return mode_; }

void LibraryBrowserViewModel::rebuild() {
    // B7: passthrough — the shared scan pipeline runs in the catalogue
    // (one store.load + both scanner queries + one tag read per track).
    // The row shapes may have changed → navigation reset.
    catalogue_.rebuild();
    // SH-1009: the files behind a row may have changed with the rescan.
    artworkPresence_.clear();
    resetNavigation();
}

void LibraryBrowserViewModel::openEntry(std::size_t index) {
    // B4: out-of-range is a rejection (house rule, NOT a silent no-op).
    if (index >= rows_.size()) {
        if (onInvalid) {
            onInvalid("openEntry: index out of range");
        }
        return;
    }
    // B4: leaf levels (tracks / playlists) have nothing to drill into —
    // a no-op there (tap semantics belong to the QML bridges).
    if (rows_[index].kind != Row::Kind::Category) {
        return;
    }
    switch (mode_) {
        case BrowserMode::Artists:
            // Two levels: artist → albums → tracks. A fresh artist drill
            // lands on the ALBUM level (selectedAlbum_ stays kNoSelection).
            if (!drilledIntoArtist_) {
                drilledIntoArtist_ = true;
                selectedArtist_ = index;
            } else {
                selectedAlbum_ = index;
            }
            break;
        case BrowserMode::Albums:
        case BrowserMode::Genres:
            // One group level → tracks.
            drilledIntoCategory_ = true;
            selectedCategory_ = index;
            break;
        case BrowserMode::Playlists:
            return;  // no group levels in this mode (B2)
    }
    repopulate();
}

bool LibraryBrowserViewModel::canGoBack() const {
    // B5: at the mode root there is nothing to go back to (both group
    // levels of the Artists chain are "backable").
    if (mode_ == BrowserMode::Artists) {
        return drilledIntoArtist_;
    }
    if (mode_ == BrowserMode::Albums || mode_ == BrowserMode::Genres) {
        return drilledIntoCategory_;
    }
    return false;
}

void LibraryBrowserViewModel::back() {
    // B5: one level up — from the track level back to the group level,
    // from the group level back to the mode root; at the root a silent
    // no-op (nothing to reject).
    if (mode_ == BrowserMode::Artists) {
        if (selectedAlbum_ != kNoSelection) {
            selectedAlbum_ = kNoSelection;  // tracks → the artist's albums
        } else {
            drilledIntoArtist_ = false;     // albums → the artists root
        }
    } else if (mode_ == BrowserMode::Albums ||
               mode_ == BrowserMode::Genres) {
        drilledIntoCategory_ = false;
    }
    repopulate();
}

std::vector<LibraryViewModel::RowCommand>
LibraryBrowserViewModel::rowCommands(std::size_t index) const {
    // B6 (SH-809 re-catalogue): category rows act on nothing; track rows
    // reuse the LibraryViewModel catalogue; playlist rows play (queue +
    // play through the SH-802 bridge). Out of range → "do not open".
    if (index >= rows_.size()) {
        return {};
    }
    switch (rows_[index].kind) {
        case Row::Kind::Category:
            return {};
        case Row::Kind::Track:
            return {LibraryViewModel::RowCommand::Play,
                    LibraryViewModel::RowCommand::AddToQueue};
        case Row::Kind::Playlist:
            return {LibraryViewModel::RowCommand::Play};
    }
    return {};
}

const std::vector<LibraryBrowserViewModel::Row>&
LibraryBrowserViewModel::rows() const {
    return rows_;
}

void LibraryBrowserViewModel::repopulate() {
    rows_.clear();

    if (mode_ == BrowserMode::Playlists) {
        // B3: Playlists mode is flat — one row per discovered .m3u path,
        // the scanner's order verbatim (label = the path itself).
        for (const std::string& path : catalogue_.playlistPaths()) {
            Row row;
            row.kind = Row::Kind::Playlist;
            row.label = path;
            row.path = path;
            rows_.push_back(std::move(row));
        }
        return;
    }

    const std::vector<CatalogueGroup>& groups =
        catalogue_.groups(mode_ == BrowserMode::Artists
                              ? CatalogueMode::Artists
                              : mode_ == BrowserMode::Albums
                                    ? CatalogueMode::Albums
                                    : CatalogueMode::Genres);

    // B7 guard: the rebuilt catalogue may no longer contain a selection —
    // reset to the mode root instead of indexing out of range.
    if (mode_ == BrowserMode::Artists && drilledIntoArtist_ &&
        selectedArtist_ >= groups.size()) {
        resetNavigation();
    }
    if (mode_ != BrowserMode::Artists && drilledIntoCategory_ &&
        selectedCategory_ >= groups.size()) {
        resetNavigation();
    }

    if (mode_ == BrowserMode::Artists) {
        // Two levels (owner decision): artist → albums → tracks.
        if (!drilledIntoArtist_) {
            for (const CatalogueGroup& artist : groups) {  // level 0
                Row row;
                row.kind = Row::Kind::Category;
                row.label = artist.label;
                // SH-1009: artist art = first picture among the artist's
                // albums, in catalogue order.
                std::vector<std::string> candidates;
                for (const CatalogueGroup& album : artist.children) {
                    candidates.insert(candidates.end(), album.paths.begin(),
                                      album.paths.end());
                }
                row.artworkPath = firstArtworkPath(candidates);
                rows_.push_back(std::move(row));
            }
            return;
        }
        const std::vector<CatalogueGroup>& albums =
            groups[selectedArtist_].children;
        if (selectedAlbum_ == kNoSelection) {  // level 1: albums
            for (const CatalogueGroup& album : albums) {
                Row row;
                row.kind = Row::Kind::Category;
                row.label = album.label;
                row.artworkPath = firstArtworkPath(album.paths);
                rows_.push_back(std::move(row));
            }
            return;
        }
        // Level 2: tracks of one album (single-album context → sibling
        // artwork fallback is safe).
        appendTrackRows(albums[selectedAlbum_].paths, true);
        return;
    }

    // Albums / Genres: one group level → tracks.
    if (!drilledIntoCategory_) {
        for (const CatalogueGroup& category : groups) {
            Row row;
            row.kind = Row::Kind::Category;
            row.label = category.label;
            row.artworkPath = firstArtworkPath(category.paths);
            rows_.push_back(std::move(row));
        }
        return;
    }
    // Albums is a single-album context; the Genres leaf mixes albums,
    // so a track may not borrow a sibling's picture there.
    appendTrackRows(groups[selectedCategory_].paths,
                    mode_ == BrowserMode::Albums);
}

void LibraryBrowserViewModel::resetNavigation() {
    // B1/B2/B7: the drill-down state back to the mode root.
    drilledIntoArtist_ = false;
    drilledIntoCategory_ = false;
    selectedArtist_ = kNoSelection;
    selectedAlbum_ = kNoSelection;
    selectedCategory_ = kNoSelection;
    repopulate();
}

void LibraryBrowserViewModel::appendTrackRows(
    const std::vector<std::string>& paths, bool singleAlbum) {
    const std::map<std::string, std::string>& titles =
        catalogue_.displayTitles();
    for (const std::string& path : paths) {
        Row row;
        row.kind = Row::Kind::Track;
        // B3: the catalogue's decided display title (no extra tag reads);
        // the path itself as the safe fallback if the catalogue was
        // rebuilt without that track.
        const std::map<std::string, std::string>::const_iterator it =
            titles.find(path);
        row.label = it != titles.end() ? it->second : path;
        row.path = path;
        // SH-1009: own picture first; borrow a sibling's only when the
        // level is one album (the Genres leaf mixes albums).
        std::vector<std::string> candidates;
        candidates.push_back(path);
        if (singleAlbum) {
            for (const std::string& sibling : paths) {
                if (sibling != path) {
                    candidates.push_back(sibling);
                }
            }
        }
        row.artworkPath = firstArtworkPath(candidates);
        rows_.push_back(std::move(row));
    }
}

std::string LibraryBrowserViewModel::firstArtworkPath(
    const std::vector<std::string>& candidates) {
    for (const std::string& path : candidates) {
        if (hasArtwork(path)) {
            return path;
        }
    }
    return std::string();
}

bool LibraryBrowserViewModel::hasArtwork(const std::string& path) {
    if (artwork_ == nullptr || path.empty()) {
        return false;
    }
    const std::map<std::string, bool>::const_iterator cached =
        artworkPresence_.find(path);
    if (cached != artworkPresence_.end()) {
        return cached->second;
    }
    const bool present = !artwork_->read(path).isEmpty();
    artworkPresence_[path] = present;
    return present;
}

}  // namespace shanty
