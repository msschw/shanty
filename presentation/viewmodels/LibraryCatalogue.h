// SH-1003 (UX-14): the library catalogue — grouping of the scanned library
// over tag metadata for the media browser's view modes (owner direction
// 2026-11-14: Artists / Albums / Genre / Playlists). Pure C++, no Qt, no
// exceptions; the QML facade adapts (SH-1005).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Decisions record (each under test in tests/library_catalogue):
//
//  1. DATA SOURCE — rebuild() composes the same scan pipeline the library
//     side uses: the working config is loaded through ILibraryStore (the
//     SAME store the settings-side editor writes — cross-visibility with
//     SH-1004), LibraryService::scan() and ::scanPlaylists() enumerate, and
//     the injected ITagReader reads tags per track AT SCAN TIME (SH-1001:
//     the store persists only the config; nothing is cached between
//     rebuilds — every track is re-read exactly once per rebuild).
//  2. GROUP SHAPE — one generic node type for all modes:
//       CatalogueGroup { label, paths, children }
//     Albums/Genres are flat (label → track paths, no children); Artists
//     are TWO levels (owner decision 2026-11-14): artist → their albums →
//     track paths, modelled as children (album groups). Playlists mode is
//     NOT grouped — SH-1002's discovery results as a flat path list in the
//     scanner's order (a playlist's contents are the m3u codec's job).
//  3. MISSING TAGS — the three "Unknown …" catalogue strings are decided
//     ONCE here and asserted verbatim: "Unknown artist", "Unknown album",
//     "Unknown genre". A clean-miss (fully untagged file) lands in all
//     three Unknown buckets as applicable; a partial tag only in the
//     missing one. The Unknown buckets participate in the normal ordering
//     (no special-casing).
//  4. TITLE FALLBACK — a track's display title is the tag title, falling
//     back to the basename without extension via Track::make (single
//     source of truth for the SH-602 rule).
//  5. ORDERING — categories (artists/albums/genres labels) alphabetical
//     CASE-INSENSITIVE; tracks within a category by display title, also
//     case-insensitive (consistent rule); album groups within an artist
//     the same way. Playlist paths keep the scanner's order verbatim.
//     Comparison is an ASCII case-fold plus byte compare (UTF-8 safe
//     byte-ordering, documented limitation for locale collation).
//  6. EMPTY LIBRARY — no candidates → every mode empty, playlists empty;
//     no special-case strings.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../core/ports/ILibraryStore.h"
#include "../core/ports/ITagReader.h"
#include "../core/services/LibraryService.h"

namespace shanty {

/// One catalogue node: a named group with its track paths (leaf modes) and
/// optional sub-groups (artists mode: the albums).
struct CatalogueGroup {
    std::string label;
    std::vector<std::string> paths;
    std::vector<CatalogueGroup> children;
};

enum class CatalogueMode { Artists, Albums, Genres };

class LibraryCatalogueViewModel {
public:
    /// @param store the library-config persistence (shared with the
    ///     settings-side editor and the library view model — SH-1004);
    /// @param library the scan pipeline (audio + playlist enumeration +
    ///     filter composition);
    /// @param reader the tag reader (SH-1001 port; clean-miss contract).
    LibraryCatalogueViewModel(ILibraryStore& store, LibraryService& library,
                              ITagReader& reader);

    /// The three "Unknown …" catalogue strings (decision 3; verbatim).
    static const char* unknownArtistLabel();
    static const char* unknownAlbumLabel();
    static const char* unknownGenreLabel();

    /// Rebuilds the catalogue from the persisted config (decision 1): one
    /// store.load(), one scan(), one scanPlaylists(), one tag read per
    /// track.
    void rebuild();

    /// The display titles decided during the last rebuild (SH-1005: the
    /// browser's track rows show them; decision 4). Exactly one entry per
    /// track of the last rebuild — tag title, or the basename fallback.
    /// Empty until the first rebuild.
    const std::map<std::string, std::string>& displayTitles() const;

    /// The grouped modes (decision 2). Empty until the first rebuild().
    const std::vector<CatalogueGroup>& groups(CatalogueMode mode) const;
    /// Playlists mode (decision 2): the discovered .m3u paths in scan order.
    const std::vector<std::string>& playlistPaths() const;

private:
    ILibraryStore& store_;
    LibraryService& library_;
    ITagReader& reader_;

    std::vector<CatalogueGroup> artists_;
    std::vector<CatalogueGroup> albums_;
    std::vector<CatalogueGroup> genres_;
    std::vector<std::string> playlists_;
    // Decision 4 output (path → display title), rebuilt each time.
    std::map<std::string, std::string> displayTitles_;
};

}  // namespace shanty
