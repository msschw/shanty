// SH-204: user-managed library configuration persisted via ILibraryStore.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "FilterRule.h"

namespace shanty {

struct LibraryConfig {
    std::vector<std::string> folders;      // managed scan roots
    std::vector<std::string> whitelist;    // force-include (optional)
    std::vector<std::string> blacklist;    // exclude

    bool operator==(const LibraryConfig& other) const {
        return folders == other.folders && whitelist == other.whitelist &&
               blacklist == other.blacklist;
    }
};

}  // namespace shanty
