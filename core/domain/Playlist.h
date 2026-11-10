// SH-304: playlist domain object — reorder / add / remove / dedupe.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "PlaylistEntry.h"

namespace shanty {

class Playlist {
public:
    const std::vector<PlaylistEntry>& entries() const { return entries_; }

    void add(const PlaylistEntry& entry);
    /// Moves the entry at `from` so it ends up at index `to`.
    /// No-op when either index is out of range or from == to.
    void moveEntry(std::size_t from, std::size_t to);
    /// Removes the entry at `index`. No-op when out of range.
    void removeAt(std::size_t index);
    void clear();
    /// Removes later duplicates (same resolved path), keeping first occurrences.
    void deduplicate();
    std::size_t count() const { return entries_.size(); }
    /// Total duration in seconds; Track::kUnknownDuration when any entry
    /// lacks a known duration.
    long long totalDurationSeconds() const;

private:
    std::vector<PlaylistEntry> entries_;
};

}  // namespace shanty
