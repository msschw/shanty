// SH-202: scan managed folders and apply whitelist/blacklist filtering.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <vector>

#include "../domain/LibraryConfig.h"
#include "../ports/ILibraryScanner.h"

namespace shanty {

class LibraryService {
public:
    explicit LibraryService(ILibraryScanner& scanner);

    /// Returns the supported tracks included by the filter rules of
    /// `config`. Order is the scanner's deterministic order, filtered.
    /// Decisions (SH-202): the scanner is invoked exactly once per scan();
    /// each candidate is kept only when it is under a managed folder, has
    /// a supported audio extension (mp3/ogg/flac), and passes
    /// FilterRule::include() (blacklist beats whitelist; empty whitelist
    /// = pass-through). A config with no folders therefore yields
    /// nothing even if the scanner returns candidates.
    std::vector<std::string> scan(const LibraryConfig& config) const;

    /// SH-1002 (SH-E10 UX-13): same composition as scan() for playlist
    /// files (.m3u/.m3u8, owner decision: a playlist = an .m3u file
    /// found within the library). The audio contract above is untouched
    /// (separate scanner query, separate composition). Phase A stub
    /// returns an empty set (red-first).
    std::vector<std::string> scanPlaylists(const LibraryConfig& config) const;

private:
    ILibraryScanner& scanner_;
};

}  // namespace shanty
