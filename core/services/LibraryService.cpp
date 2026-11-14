// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryService.h"

#include "../domain/AudioFormat.h"
#include "../domain/FilterRule.h"

namespace shanty {

LibraryService::LibraryService(ILibraryScanner& scanner) : scanner_(scanner) {}

std::vector<std::string> LibraryService::scan(
    const LibraryConfig& config) const {
    // The scanner is invoked exactly once per scan(); it only enumerates.
    const std::vector<std::string> candidates = scanner_.scan(config.folders);

    const FilterRule filter(config.whitelist, config.blacklist);
    std::vector<std::string> tracks;
    tracks.reserve(candidates.size());
    for (const std::string& candidate : candidates) {
        // Composition rule: candidate → under a managed folder →
        // supported extension → FilterRule::include().
        bool underManagedFolder = false;
        for (const std::string& folder : config.folders) {
            if (isUnderPath(candidate, folder)) {
                underManagedFolder = true;
                break;
            }
        }
        if (!underManagedFolder) {
            continue;
        }
        if (!isSupportedAudioPath(candidate)) {
            continue;
        }
        if (!filter.include(candidate)) {
            continue;
        }
        tracks.push_back(candidate);
    }
    return tracks;
}

// SH-1002 Phase A stub: always empty so the red-first library_scan
// cases fail by assertion. Phase B composes: managed folder → playlist
// extension → FilterRule (identical rule stack to scan()).
std::vector<std::string> LibraryService::scanPlaylists(
    const LibraryConfig& config) const {
    (void)config;
    return {};
}
}  // namespace shanty
