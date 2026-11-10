// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PathResolver.h"

#include "Playlist.h"

namespace shanty {
// TODO(SH-305 Phase B): implement the documented resolution rule.
Playlist resolveEntryPaths(const Playlist&, const std::string&) {
    return Playlist();  // unimplemented (tests must fail red)
}
}  // namespace shanty
