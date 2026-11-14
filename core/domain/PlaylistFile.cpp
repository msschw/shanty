// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaylistFile.h"

namespace shanty {

// SH-1002 Phase A stub: always false (clean miss) so the red-first
// suites fail by assertion. Phase B implements the case-insensitive
// .m3u/.m3u8 suffix check.
bool isSupportedPlaylistPath(const std::string& path) {
    (void)path;
    return false;
}

}  // namespace shanty
