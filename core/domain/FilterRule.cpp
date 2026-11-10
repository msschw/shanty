// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "FilterRule.h"

namespace shanty {
// TODO(SH-201 Phase B): implement prefix matching and the whitelist /
// blacklist composition rule described in the header.
FilterRule::FilterRule(std::vector<std::string>, std::vector<std::string>) {}

bool FilterRule::include(const std::string&) const {
    return false;  // unimplemented (tests must fail red)
}

bool isUnderPath(const std::string&, const std::string&) {
    return false;  // unimplemented (tests must fail red)
}
}  // namespace shanty
