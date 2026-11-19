// SH-1009 (UX-20): the concrete embedded-artwork reader.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "core/ports/IArtworkReader.h"

namespace shanty {

/// Extracts embedded cover art via taglib.
///
/// Contract (SH-1009):
///   - Clean miss, never a throw: an absent, unreadable, unsupported or
///     picture-less file yields an empty Artwork (same rule as
///     ITagReader).
///   - Preference: a picture typed "front cover" wins over other picture
///     types; among equals the first one in the tag's order is used.
///   - Containers: MP3 (ID3v2 APIC), FLAC (PICTURE blocks), Ogg Vorbis
///     (Xiph METADATA_BLOCK_PICTURE). Other containers cleanly miss
///     (no cover extraction yet).
///   - MIME type: the tag's value when present; the consumer sniffs the
///     bytes otherwise.
///   - taglib 2.x union API only (host 2.3.2, SDK target 2.0.2).
class TagLibArtworkReader : public IArtworkReader {
public:
    TagLibArtworkReader() = default;

    Artwork read(const std::string& path) override;

private:
    TagLibArtworkReader(const TagLibArtworkReader&) = delete;
    TagLibArtworkReader& operator=(const TagLibArtworkReader&) = delete;
};

}  // namespace shanty
