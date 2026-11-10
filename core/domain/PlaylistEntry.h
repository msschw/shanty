// SH-201/SH-304: a single playlist entry (path + optional metadata).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "Track.h"  // for the shared kUnknownDuration constant

namespace shanty {

struct PlaylistEntry {
    /// Optional paths: the first is the entry path as stored in the file,
    /// the second (when resolved), the absolute target. See PathResolver.
    std::string path;

    /// Empty when the playlist carries no #EXTINF title.
    std::string title;

    /// Track::kUnknownDuration when no #EXTINF duration is present.
    int durationSeconds = Track::kUnknownDuration;

    bool operator==(const PlaylistEntry& other) const {
        return path == other.path && title == other.title &&
               durationSeconds == other.durationSeconds;
    }
};

}  // namespace shanty
