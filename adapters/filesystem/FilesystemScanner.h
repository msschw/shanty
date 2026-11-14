// SH-203 adapter. Realized contract: scan() recursively enumerates the given
// folders and returns only supported audio paths (mp3/ogg/flac,
// case-insensitive, via shanty::isSupportedAudioPath), deduplicated and
// sorted deterministically by full path string. Non-existing roots
// contribute nothing; unreadable subtrees are skipped without crashing.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/ILibraryScanner.h"

namespace shanty {

class FilesystemScanner : public ILibraryScanner {
public:
    std::vector<std::string> scan(
        const std::vector<std::string>& folders) override;

    // SH-1002 (SH-E10 UX-13): playlist enumeration — see the port for
    // the contract. Phase A stub returns nothing (red-first).
    std::vector<std::string> scanPlaylists(
        const std::vector<std::string>& folders) override;
};

}  // namespace shanty
