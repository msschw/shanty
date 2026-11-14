// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "FilesystemScanner.h"

#include "core/domain/AudioFormat.h"
#include "core/domain/PlaylistFile.h"

#include <QDir>
#include <QDirIterator>

#include <algorithm>

namespace shanty {

namespace {

// SH-1002 Phase B: shared enumeration core for the two scanner queries.
// Same contract as scan()'s documented behaviour (see the comment below):
// recursive QDirIterator walk, per-candidate predicate, deduplicated and
// sorted deterministically by full path string.
std::vector<std::string> enumerate(const std::vector<std::string>& folders,
                                   bool (*keep)(const std::string&)) {
    std::vector<std::string> paths;

    for (const std::string& folder : folders) {
        const QString root = QString::fromStdString(folder);
        // A non-existing (or non-directory) root contributes nothing and
        // must never crash; unreadable subdirectories are silently skipped
        // by QDirIterator.
        if (!QDir(root).exists()) {
            continue;
        }
        QDirIterator iterator(
            root,
            QDir::Files | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const std::string candidate =
                iterator.next().toStdString();
            if (keep(candidate)) {
                paths.push_back(candidate);
            }
        }
    }

    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}

}  // namespace

// SH-203 Phase B: recursive enumeration under each managed folder via
// QDirIterator, filtered to the supported audio extensions, deduplicated,
// and returned in a deterministic sorted order.
//
// Port note (realized contract): `ILibraryScanner`'s port documentation says
// the scanner returns "candidate" paths and leaves extension classification
// to LibraryService's filtering — but the ENUMERATION contract also presents
// the scanner as the agent listing audio files. This adapter therefore
// already prunes to the supported set (mp3/ogg/flac, case-insensitive, via
// `shanty::isSupportedAudioPath` from core) because the SH-203 tests lock in
// that behaviour. LibraryService's whitelist/blacklist filtering composes on
// top of this candidate list unchanged.
//
// Deterministic order: results are collected from all folders and sorted by
// their full UTF-8 path string (plain lexicographic), then duplicates (from
// overlapping root folders) are removed. This makes repeated scans and
// scans of overlapping trees byte-for-byte reproducible regardless of the
// filesystem's native directory iteration order.

std::vector<std::string> FilesystemScanner::scan(
    const std::vector<std::string>& folders) {
    return enumerate(folders, isSupportedAudioPath);
}

// SH-1002 Phase B: playlist enumeration — identical walk/dedup/sort
// contract as scan(), filtered to .m3u/.m3u8 (isSupportedPlaylistPath).
std::vector<std::string> FilesystemScanner::scanPlaylists(
    const std::vector<std::string>& folders) {
    return enumerate(folders, isSupportedPlaylistPath);
}

}  // namespace shanty
