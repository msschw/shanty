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

private:
    ILibraryScanner& scanner_;
};

}  // namespace shanty
