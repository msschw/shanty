// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryService.h"

namespace shanty {
// TODO(SH-202 Phase B): composition rule implemented here, scanner only
// enumerates:
//   candidate → supported extension? → FilterRule::include()?

LibraryService::LibraryService(ILibraryScanner& scanner) : scanner_(scanner) {}

std::vector<std::string> LibraryService::scan(const LibraryConfig&) const {
    return std::vector<std::string>();  // unimplemented (tests must fail red)
}
}  // namespace shanty
