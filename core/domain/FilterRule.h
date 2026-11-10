// SH-201: whitelist/blacklist filter (charter: "manageable library").
// Rule (documented, tested): a path is included when it is under a
// managed folder (checked by the caller), NOT blacklisted, and whitelisted
// unless no whitelist entry exists (empty whitelist = include all).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace shanty {

class FilterRule {
public:
    FilterRule() = default;
    FilterRule(std::vector<std::string> whitelist,
               std::vector<std::string> blacklist);

    bool include(const std::string& path) const;

    const std::vector<std::string>& whitelist() const { return whitelist_; }
    const std::vector<std::string>& blacklist() const { return blacklist_; }

private:
    std::vector<std::string> whitelist_;
    std::vector<std::string> blacklist_;
};

/// True when `path` equals or is below `root` (case-insensitive on the
/// extension only? No: prefix comparison is case-sensitive on systems
/// with case-sensitive file systems, case-insensitive on extensions).
bool isUnderPath(const std::string& path, const std::string& root);

}  // namespace shanty
