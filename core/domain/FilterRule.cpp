// SH-201/SH-202: whitelist/blacklist composition and prefix matching.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "FilterRule.h"

namespace shanty {

FilterRule::FilterRule(std::vector<std::string> whitelist,
                       std::vector<std::string> blacklist)
    : whitelist_(std::move(whitelist)), blacklist_(std::move(blacklist)) {}

bool FilterRule::include(const std::string& path) const {
    // Blacklist always wins: any hit excludes the path outright.
    for (const std::string& entry : blacklist_) {
        if (isUnderPath(path, entry)) {
            return false;
        }
    }
    // Empty whitelist = include everything that survived the blacklist.
    if (whitelist_.empty()) {
        return true;
    }
    for (const std::string& entry : whitelist_) {
        if (isUnderPath(path, entry)) {
            return true;
        }
    }
    return false;
}

bool isUnderPath(const std::string& path, const std::string& root) {
    if (root.empty()) {
        return false;
    }
    // Exact match (entry names the file itself).
    if (path == root) {
        return true;
    }
    // Prefix match: the candidate must continue the root either with a
    // path separator ('/' — root is a folder and path is below it) or
    // with an extension dot ('.' — the whitelist entry is a file stem
    // such as "/music/a", which covers "/music/a.mp3").
    if (path.size() > root.size() && path.compare(0, root.size(), root) == 0) {
        const char next = path[root.size()];
        return next == '/' || next == '.';
    }
    return false;
}
}  // namespace shanty
