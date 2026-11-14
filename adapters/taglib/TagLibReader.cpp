// SH-1001 Phase A compile-fix stub (SH-E10 UX-12). See the header
// comment: read() always misses so the red suite fails by assertion,
// never by crash or compile error.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "TagLibReader.h"

namespace shanty {

TagMetadata TagLibReader::read(const std::string& path) {
    (void)path;  // STUB — Phase B implements the real read.
    return TagMetadata();
}

}  // namespace shanty
