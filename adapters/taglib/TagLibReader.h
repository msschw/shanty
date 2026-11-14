// SH-1001 Phase B (SH-E10 UX-12): the concrete taglib reader.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "core/ports/ITagReader.h"

namespace shanty {

/// Reads tag metadata via taglib (the image's audio-metadata library).
/// Contract notes (SH-1001):
///   - Clean miss, never a throw: an absent, unreadable, unsupported
///     or unparseable file yields an empty TagMetadata (isValid() is
///     checked BEFORE any tag access — taglib returns tag pointers of
///     NULL for unparseable files).
///   - taglib 2.x union API only (FileRef/Tag/AudioProperties) — the
///     SDK target carries 2.0.2 while the host carries 2.3.2; the union
///     surface is identical across both.
///   - Strings cross the boundary as UTF-8 (toCString(true)); empty
///     fields mean "no such tag".
///   - durationSeconds comes from the audio properties (present even
///     when no tag exists); -1 when the properties are unavailable.
class TagLibReader : public ITagReader {
public:
    TagLibReader() = default;

    TagMetadata read(const std::string& path) override;

private:
    TagLibReader(const TagLibReader&) = delete;
    TagLibReader& operator=(const TagLibReader&) = delete;
};

}  // namespace shanty
