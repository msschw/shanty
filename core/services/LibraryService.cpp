// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryService.h"

#include "../domain/AudioFormat.h"
#include "../domain/PlaylistFile.h"
#include "../domain/FilterRule.h"

namespace shanty {

namespace {

// SH-202/SH-1002 composition rule, shared by scan() and scanPlaylists():
// candidate → under a managed folder → extension predicate →
// FilterRule::include() (blacklist beats whitelist; empty whitelist =
// pass-through). Single decision point per SH-202's composition rule.
std::vector<std::string> applyComposition(
    const std::vector<std::string>& candidates, const LibraryConfig& config,
    bool (*supportedPath)(const std::string&)) {
    const FilterRule filter(config.whitelist, config.blacklist);
    std::vector<std::string> result;
    result.reserve(candidates.size());
    for (const std::string& candidate : candidates) {
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
        if (!supportedPath(candidate)) {
            continue;
        }
        if (!filter.include(candidate)) {
            continue;
        }
        result.push_back(candidate);
    }
    return result;
}

}  // namespace

LibraryService::LibraryService(ILibraryScanner& scanner) : scanner_(scanner) {}

std::vector<std::string> LibraryService::scan(
    const LibraryConfig& config) const {
    // The scanner is invoked exactly once per scan(); it only enumerates.
    const std::vector<std::string> candidates = scanner_.scan(config.folders);
    return applyComposition(candidates, config, isSupportedAudioPath);
}

// SH-1002 Phase B: identical rule stack over the scanner's separate
// playlist enumeration (.m3u/.m3u8 — owner decision: a playlist is an
// .m3u file found within the library). The audio scan() above is
// untouched.
std::vector<std::string> LibraryService::scanPlaylists(
    const LibraryConfig& config) const {
    const std::vector<std::string> candidates =
        scanner_.scanPlaylists(config.folders);
    return applyComposition(candidates, config, isSupportedPlaylistPath);
}

}  // namespace shanty
