// SH-1003 (UX-14): Phase B — the real grouping implementation of the
// contract locked by tests/library_catalogue (decisions 1–6 in
// LibraryCatalogue.h).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryCatalogue.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>
#include "../core/domain/Track.h"

namespace shanty {
namespace {

/// Decision 5: case-insensitive ordering — an ASCII case-fold per byte plus
/// byte compare (UTF-8 safe: bytes >= 0x80 are left untouched, so the
/// ordering is a documented non-locale byte order).
std::string foldAscii(const std::string& text) {
    std::string folded = text;
    for (std::size_t i = 0; i < folded.size(); ++i) {
        if (folded[i] >= 'A' && folded[i] <= 'Z') {
            folded[i] = static_cast<char>(folded[i] - 'A' + 'a');
        }
    }
    return folded;
}

struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const {
        return foldAscii(a) < foldAscii(b);
    }
};

/// A track leaf: display title (decision 4) + path, ordered by title.
struct TrackLeaf {
    std::string title;
    std::string path;
};

/// Sorts leaves by display title, case-insensitively; the path breaks
/// ties so the result is deterministic for equal titles.
std::vector<std::string> orderedPaths(std::vector<TrackLeaf> leaves) {
    std::sort(leaves.begin(), leaves.end(),
              [](const TrackLeaf& a, const TrackLeaf& b) {
                  const std::string foldA = foldAscii(a.title);
                  const std::string foldB = foldAscii(b.title);
                  if (foldA != foldB) {
                      return foldA < foldB;
                  }
                  return a.path < b.path;
              });
    std::vector<std::string> paths;
    paths.reserve(leaves.size());
    for (const TrackLeaf& leaf : leaves) {
        paths.push_back(leaf.path);
    }
    return paths;
}

}  // namespace

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

void LibraryCatalogueViewModel::rebuild() {
    // Decision 1: the scan pipeline runs fresh — the working config is
    // re-read through the store (cross-visibility read path, SH-1004),
    // both scanner queries run once, and every track's tags are re-read
    // exactly once at scan time (decision 1; tests assert the counts).
    const LibraryConfig config = store_.load();
    const std::vector<std::string> tracks = library_.scan(config);
    playlists_ = library_.scanPlaylists(config);

    // Flat group builders (decision 2: Albums/Genres are one level);
    // case-insensitive label ordering falls out of the map comparator.
    std::map<std::string, std::vector<TrackLeaf>, CaseInsensitiveLess>
        albumLeaves;
    std::map<std::string, std::vector<TrackLeaf>, CaseInsensitiveLess>
        genreLeaves;
    // Artists are two levels: artist → album → leaves.
    std::map<std::string,
             std::map<std::string, std::vector<TrackLeaf>,
                      CaseInsensitiveLess>,
             CaseInsensitiveLess>
        artistAlbums;

    for (const std::string& path : tracks) {
        const TagMetadata meta = reader_.read(path);
        // Decision 4: display title = tag title, falling back to the
        // basename without extension (Track::make rule).
        std::string title = meta.title;
        if (title.empty()) {
            title = Track::make(path).title();
        }
        // Decision 3: missing tags land in the Unknown buckets (the same
        // verbatim strings as the observation API below — no special-
        // casing of the buckets anywhere).
        const std::string artist =
            meta.artist.empty() ? unknownArtistLabel() : meta.artist;
        const std::string album =
            meta.album.empty() ? unknownAlbumLabel() : meta.album;
        const std::string genre =
            meta.genre.empty() ? unknownGenreLabel() : meta.genre;

        const TrackLeaf leaf{title, path};
        albumLeaves[album].push_back(leaf);
        genreLeaves[genre].push_back(leaf);
        artistAlbums[artist][album].push_back(leaf);
        displayTitles_[path] = title;  // SH-1005: browser track rows
    }

    // Albums / Genres: flat groups, label-ordered, leaf-ordered.
    albums_.clear();
    genres_.clear();
    for (const auto& entry : albumLeaves) {
        CatalogueGroup group;
        group.label = entry.first;
        group.paths = orderedPaths(entry.second);
        albums_.push_back(std::move(group));
    }
    for (const auto& entry : genreLeaves) {
        CatalogueGroup group;
        group.label = entry.first;
        group.paths = orderedPaths(entry.second);
        genres_.push_back(std::move(group));
    }

    // Artists: two levels — each artist group carries its albums as
    // children (label-ordered); the artist node itself has no leaf paths.
    artists_.clear();
    for (const auto& artistEntry : artistAlbums) {
        CatalogueGroup artist;
        artist.label = artistEntry.first;
        for (const auto& albumEntry : artistEntry.second) {
            CatalogueGroup album;
            album.label = albumEntry.first;
            album.paths = orderedPaths(albumEntry.second);
            artist.children.push_back(std::move(album));
        }
        artists_.push_back(std::move(artist));
    }
}

const std::vector<CatalogueGroup>& LibraryCatalogueViewModel::groups(
    CatalogueMode mode) const {
    switch (mode) {
        case CatalogueMode::Artists:
            return artists_;
        case CatalogueMode::Albums:
            return albums_;
        case CatalogueMode::Genres:
            return genres_;
    }
    static const std::vector<CatalogueGroup> empty;
    return empty;
}

const std::vector<std::string>& LibraryCatalogueViewModel::playlistPaths()
    const {
    return playlists_;
}

const std::map<std::string, std::string>&
LibraryCatalogueViewModel::displayTitles() const {
    return displayTitles_;
}

}  // namespace shanty

