// SH-305 Phase B: resolve playlist entry paths at the import boundary
// (design D3: the domain receives resolved plain paths only).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PathResolver.h"

#include "Playlist.h"

namespace shanty {
namespace {

/// Decimal/hex helper: value of `c` as a hex digit, or -1 when not hex.
int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/// Decodes every well-formed `%XX` escape into its raw byte. Malformed
/// sequences (a trailing `%`, or `%` followed by a non-hex digit) are kept
/// verbatim: losslessly re-serialisable input beats silent data loss.
/// UTF-8 bytes are copied through unchanged, so `%C3%B6` yields the raw
/// 0xC3 0xB6 bytes of "ö" (no Qt, no charset conversion — pure bytes).
std::string percentDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        const char c = encoded[i];
        if (c != '%') {
            decoded += c;
            continue;
        }
        if (i + 2 < encoded.size()) {
            const int high = hexValue(encoded[i + 1]);
            const int low = hexValue(encoded[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded += static_cast<char>((high << 4) | low);
                i += 2;
                continue;
            }
        }
        decoded += '%';  // malformed escape: keep verbatim
    }
    return decoded;
}

/// True when the entry addresses a "file://" URL (literal lowercase scheme,
/// as emitted by standard producers; anything else is treated as a plain
/// relative or absolute path per the documented rule).
bool isFileUrl(const std::string& path) {
    return path.rfind("file://", 0) == 0;
}

/// Converts "file://[host]/path-part" to "/path-part" (host prefix removed,
/// covering the empty host of "file:///..." too). A URL without a path part
/// after the host resolves to the empty string.
std::string fileUrlToPath(const std::string& url) {
    const std::size_t schemeEnd = 7;  // std::string("file://").size()
    const std::size_t pathStart = url.find('/', schemeEnd);
    if (pathStart == std::string::npos) return std::string();
    return percentDecode(url.substr(pathStart));
}

/// Joins a relative entry onto `baseDir` with a separator. A leading "./"
/// segment of the entry is dropped so "./other.flac" resolves cleanly.
std::string joinRelative(const std::string& baseDir, const std::string& entry) {
    const char* const dotPrefix = "./";
    std::string relative = entry;
    if (relative.compare(0, 2, dotPrefix) == 0) {
        relative = relative.substr(2);
    }
    std::string joined = baseDir;
    if (!joined.empty() && joined.back() != '/' && !relative.empty()) {
        joined += '/';
    }
    joined += relative;
    return joined;
}

/// Applies the documented resolution rule to one entry path.
std::string resolveOnePath(const std::string& path, const std::string& baseDir) {
    if (isFileUrl(path)) {
        return fileUrlToPath(path);
    }
    if (!path.empty() && path.front() == '/') {
        return path;  // absolute plain path passes through unchanged
    }
    if (baseDir.empty()) {
        return path;  // no base to resolve against: relative entry unchanged
    }
    return joinRelative(baseDir, path);
}

}  // namespace

Playlist resolveEntryPaths(const Playlist& playlist, const std::string& baseDir) {
    Playlist resolved;
    // Copy-construct every entry so metadata (title, duration) is preserved,
    // then rewrite only the path field. Never mutates the argument.
    for (const PlaylistEntry& entry : playlist.entries()) {
        PlaylistEntry resolvedEntry = entry;
        resolvedEntry.path = resolveOnePath(entry.path, baseDir);
        resolved.add(resolvedEntry);
    }
    return resolved;
}

}  // namespace shanty
