// SH-1001 port (SH-E10 UX-12, Dependency Inversion — ISSUES.md §1.1/§1.2):
// tag metadata reading goes through a port + adapter (like
// IPlaylistCodec/ILibraryScanner/IAutoEqReader) so the concrete reader
// (taglib today, another backend later) can change without touching the
// domain or the catalogue composition (Open/Closed; D5-style data behind
// an abstraction).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "../domain/TagMetadata.h"

namespace shanty {

/// Reads tag metadata (title/artist/album/genre + duration) from an
/// audio file path. Implementations MUST NOT throw: an absent,
/// unreadable or unsupported file yields the clean miss (an empty
/// TagMetadata per TagMetadata.h) — callers decide fallbacks (e.g.
/// title = basename) from the empty fields.
class ITagReader {
public:
    virtual ~ITagReader() = default;

    virtual TagMetadata read(const std::string& path) = 0;
};

}  // namespace shanty
