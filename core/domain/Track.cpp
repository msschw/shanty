// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "Track.h"

#include <cstddef>
#include <string>

namespace shanty {
namespace {

/// Extracts the extension suffix of a path (after the final dot), or "" when
/// the path has no such suffix. A dot that only appears inside a directory
/// component ("/music.v2/song") does not count.
std::string extensionOf(const std::string& path) {
    const std::size_t lastDot = path.find_last_of('.');
    if (lastDot == std::string::npos || lastDot + 1 >= path.size()) {
        return std::string();
    }
    const std::string suffix = path.substr(lastDot + 1);
    if (suffix.find('/') != std::string::npos) {
        return std::string();
    }
    return suffix;
}

/// Returns the basename (final path component) of a plain path.
std::string basenameOf(const std::string& path) {
    const std::size_t lastSlash = path.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return path;
    }
    if (lastSlash + 1 >= path.size()) {
        return std::string();
    }
    return path.substr(lastSlash + 1);
}

}  // namespace

Track Track::make(const std::string& path) {
    Track track;
    track.format_ = classifyExtension(extensionOf(path));
    if (track.format_ == AudioFormat::Unsupported) {
        return track;  // invalid Track: unsupported extension
    }
    // Relative paths pass through unchanged as plain paths (design D3 —
    // no QUrl involvement anywhere in core).
    track.path_ = path;

    // Title falls back to the basename without extension.
    std::string title = basenameOf(path);
    const std::size_t lastDot = title.find_last_of('.');
    if (lastDot != std::string::npos && lastDot > 0) {
        title.erase(lastDot);
    }
    track.title_ = title;

    // Duration stays unknown until metadata is read.
    track.durationSeconds_ = kUnknownDuration;
    return track;
}

}  // namespace shanty
