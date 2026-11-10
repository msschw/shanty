// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "FilesystemScanner.h"

namespace shanty {
// TODO(SH-203 Phase B): recursive enumeration, sorted deterministic order,
// skip unreadable directories (no crash), supported extensions only.

std::vector<std::string> FilesystemScanner::scan(
    const std::vector<std::string>&) {
    return std::vector<std::string>();  // unimplemented (tests must fail red)
}
}  // namespace shanty
