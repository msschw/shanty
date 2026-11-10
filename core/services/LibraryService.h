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
    std::vector<std::string> scan(const LibraryConfig& config) const;

private:
    ILibraryScanner& scanner_;
};

}  // namespace shanty
