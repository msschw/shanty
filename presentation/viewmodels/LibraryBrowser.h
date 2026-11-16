// SH-1005 (UX-16): the media-browser state layer — the page-level facade
// contract over the SH-1003 catalogue: view-mode selection, the per-mode
// group lists, and the drill-down state (Artists: artist → albums →
// tracks; Albums/Genres: category → tracks; Playlists: flat). Pure C++,
// no Qt, no exceptions; the QML page (Phase B) holds a reference only.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Decisions record (each under test in tests/library_catalogue):
//
//  B1 MODE SELECTOR — setMode switches the view mode and resets the
//     drill-down to the mode ROOT; the initial mode is Artists (the
//     owner's first listed mode). No other state is touched.
//  B2 NAVIGATION — at most two group levels per mode (owner decision:
//     Artists two levels, Albums/Genres one); Playlists has none. The
//     internal state is the mode + the selected group indices; both
//     setMode and rebuild reset it.
//  B3 ROWS — rows() is a snapshot (house getter idiom): at a group level
//     one Category row per catalogue group; at a leaf level one Track row
//     per track path (label = the catalogue's display title — NO extra
//     tag reads); in Playlists mode one Playlist row per discovered .m3u
//     path (label = the path itself).
//  B4 openEntry — a valid CATEGORY row index drills one level down; an
//     out-of-range index is rejected (NOT silently ignored) with the
//     house catalogue string "openEntry: index out of range". Track and
//     Playlist rows are not openEntry's business — their tap semantics
//     (play / queue+play) live in the QML bridges over the existing
//     facades.
//  B5 back — one level up; at the mode root back() is a no-op (canGoBack
//     is false there).
//  B6 ROW COMMANDS (SH-809 re-catalogue, browser rows) — Category rows
//     have no commands (nothing to act on); Track rows offer
//     {Play, AddToQueue} (the LibraryViewModel catalogue, reused);
//     Playlist rows offer {Play} (queue + play through the SH-802 bridge,
//     no auto-navigation to the editor). Out-of-range indices → {}
//     (the "do not open" signal).
//  B7 rebuild — passthrough to the catalogue (the shared scan pipeline
//     runs there) + navigation reset (the row shapes may have changed).
//     No independent scanning here.
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "LibraryCatalogue.h"
#include "LibraryViewModel.h"  // RowCommand catalogue reuse (SH-809)

namespace shanty {

enum class BrowserMode { Artists, Albums, Genres, Playlists };

class LibraryBrowserViewModel {
public:
    /// One browsable row of the current level (B3).
    struct Row {
        enum class Kind { Category, Track, Playlist };

        Kind kind = Kind::Category;
        std::string label;  // category label / display title / playlist path
        std::string path;   // Track + Playlist rows; "" for Category rows
    };

    /// @param catalogue the SH-1003 grouping layer (rebuilt by the shared
    ///     pipeline; this class never scans itself).
    explicit LibraryBrowserViewModel(LibraryCatalogueViewModel& catalogue);

    void setMode(BrowserMode mode);  // B1
    BrowserMode mode() const;
    /// Passthrough to the catalogue (B7) + navigation reset.
    void rebuild();

    /// The current level's rows (B3; snapshot — empty until the first
    /// rebuild).
    const std::vector<Row>& rows() const;

    /// Drills one level down (B4). Rejections via onInvalid.
    void openEntry(std::size_t index);
    bool canGoBack() const;  // B5
    void back();             // B5

    /// SH-809 browser-row catalogue (B6).
    std::vector<LibraryViewModel::RowCommand> rowCommands(
        std::size_t index) const;

    /// Rejection channel (house pattern: shared rejection channel,
    /// mirroring PlaybackController::onInvalidCommand).
    std::function<void(const char* reason)> onInvalid;

private:
    LibraryCatalogueViewModel& catalogue_;

    // B2: the drill-down chain (indices into the catalogue groups). The
    // album selection starts "unset" (a size that no catalogue has) — a
    // fresh artist drill lands on the ALBUM level, not on album 0.
    BrowserMode mode_ = BrowserMode::Artists;
    static const std::size_t kNoSelection = static_cast<std::size_t>(-1);
    std::size_t selectedArtist_ = kNoSelection;    // Artists level 1
    std::size_t selectedAlbum_ = kNoSelection;     // Artists level 2
    std::size_t selectedCategory_ = kNoSelection;  // Albums / Genres level 1
    bool drilledIntoArtist_ = false;
    bool drilledIntoCategory_ = false;

    std::vector<Row> rows_;

    void repopulate();

    /// B3 leaf rows: one Track row per path (label = the catalogue's
    /// display title, safe fallback the path itself).
    void appendTrackRows(const std::vector<std::string>& paths);

    /// B1/B2/B7: drill-down state back to the mode root.
    void resetNavigation();
};

}  // namespace shanty
