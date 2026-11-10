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
};

}  // namespace shanty
