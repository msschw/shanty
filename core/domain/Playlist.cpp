// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "Playlist.h"

namespace shanty {
// TODO(SH-304 Phase B): implement the documented mutation semantics.

void Playlist::add(const PlaylistEntry& entry) {
    // Minimal data-population helper (SH-301 Phase B exception): appends the
    // entry verbatim; SH-304's tests do not depend on it preparing anything
    // beyond count()/entries() semantics. All other mutation methods and the
    // TODO above remain owned by SH-304.
    entries_.push_back(entry);
}

void Playlist::moveEntry(std::size_t, std::size_t) {}  // unimplemented (tests must fail red)

void Playlist::removeAt(std::size_t) {}  // unimplemented (tests must fail red)

void Playlist::clear() {}  // unimplemented (tests must fail red)

void Playlist::deduplicate() {}  // unimplemented (tests must fail red)

long long Playlist::totalDurationSeconds() const { return -2; }  // unimplemented (tests must fail red)
}  // namespace shanty
