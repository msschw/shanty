// SH-202 port. Returns candidate audio paths below the given folders,
// recursively, sorted deterministically. The scanner is *not* responsible
// for whitelist/blacklist filtering — that is LibraryService's job.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace shanty {

class ILibraryScanner {
public:
    virtual ~ILibraryScanner() = default;

    virtual std::vector<std::string> scan(
        const std::vector<std::string>& folders) = 0;

    // SH-1002 (SH-E10 UX-13): playlist discovery as a SEPARATE query —
    // the audio scan() contract is untouched (Interface Segregation:
    // audio consumers see the same port method as before). Same
    // enumeration contract: recursive, deduplicated, deterministically
    // sorted; the scanner enumerates candidate playlist files
    // (.m3u/.m3u8) and leaves classification/filtering to
    // LibraryService::scanPlaylists.
    virtual std::vector<std::string> scanPlaylists(
        const std::vector<std::string>& folders) = 0;
};

}  // namespace shanty
