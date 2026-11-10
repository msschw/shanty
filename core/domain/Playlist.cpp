// SH-304 Phase B: playlist domain mutations — reorder / add / remove / dedupe.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "Playlist.h"

#include <unordered_set>
#include <utility>

namespace shanty {

void Playlist::add(const PlaylistEntry& entry) {
    // Minimal data-population helper (SH-301 Phase B exception): appends the
    // entry verbatim; SH-304's tests do not depend on it preparing anything
    // beyond count()/entries() semantics. All other mutation methods and the
    // TODO above remain owned by SH-304.
    entries_.push_back(entry);
}

void Playlist::moveEntry(std::size_t from, std::size_t to) {
    // Only valid index ranges mutate: out-of-range `from` or `to` and
    // from == to are no-ops (per header docs and tst_playlist_ops).
    if (from >= entries_.size() || to >= entries_.size() || from == to)
        return;
    PlaylistEntry entry = entries_[from];
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(from));
    // Erase-then-insert: an entry moved earlier shifts later indices right.
    entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(to),
                    std::move(entry));
}

void Playlist::removeAt(std::size_t index) {
    if (index >= entries_.size())
        return;  // out of range: no crash, no change
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
}

void Playlist::clear() {
    entries_.clear();
}

void Playlist::deduplicate() {
    // Keep the FIRST occurrence per identical path; drop later duplicates.
    std::unordered_set<std::string> seen;
    seen.reserve(entries_.size());
    std::vector<PlaylistEntry> kept;
    kept.reserve(entries_.size());
    for (PlaylistEntry& entry : entries_) {
        if (seen.insert(entry.path).second)
            kept.push_back(std::move(entry));
    }
    entries_ = std::move(kept);
}

long long Playlist::totalDurationSeconds() const {
    // Empty playlist: 0. Any entry lacking a known duration flags the whole
    // total as unknown (Track::kUnknownDuration).
    long long total = 0;
    for (const PlaylistEntry& entry : entries_) {
        if (entry.durationSeconds == Track::kUnknownDuration)
            return Track::kUnknownDuration;
        total += entry.durationSeconds;
    }
    return total;
}

}  // namespace shanty
