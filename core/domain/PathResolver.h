// SH-305: Resolve playlist entry paths at the import boundary (design D3:
// the domain receives resolved plain paths only).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Documented rule (tested in tst_m3u_resolve):
//   - relative entries resolve against the playlist's own directory,
//   - absolute paths pass through unchanged,
//   - "file://"-scheme URLs are converted to plain paths (URL-decoded,
//     host prefix removed).
//   - resolution result replaces the entry's path; baseDir "" yields the
//     entries unchanged except file:// conversion.
#pragma once

#include <string>

namespace shanty {

class Playlist;

class Playlist;

/// Resolves every entry of `playlist` against `baseDir` and returns the
/// resolved playlist (never mutates the argument).
Playlist resolveEntryPaths(const Playlist& playlist, const std::string& baseDir);

}  // namespace shanty
