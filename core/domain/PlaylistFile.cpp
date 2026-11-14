// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaylistFile.h"

namespace shanty {

namespace {

// Extension-only check (the stem may contain dots — e.g. "set.v2.m3u"
// is a playlist, "set.m3u.bak" is not). Case-insensitive: the tail is
// ASCII-folded to lowercase and compared against the lowercase suffix.
bool endsWithCaseInsensitive(const std::string& path,
                             const std::string& lowercaseSuffix) {
    if (path.size() < lowercaseSuffix.size()) {
        return false;
    }
    const std::size_t offset = path.size() - lowercaseSuffix.size();
    for (std::size_t i = 0; i < lowercaseSuffix.size(); ++i) {
        char c = path[offset + i];
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
        if (c != lowercaseSuffix[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool isSupportedPlaylistPath(const std::string& path) {
    // Matches isSupportedAudioPath's extension-only semantics; the dot
    // is part of the matched suffix so a bare "m3u" stem never passes.
    return endsWithCaseInsensitive(path, ".m3u") ||
           endsWithCaseInsensitive(path, ".m3u8");
}

}  // namespace shanty
