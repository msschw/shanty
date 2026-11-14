// SH-1001 Phase A compile-fix stub (SH-E10 UX-12): the adapter type
// exists so the tag_reader suite compiles; read() deliberately misses
// everything (an empty TagMetadata) — the suite's value assertions fail
// against THIS stub until Phase B wires taglib into it. The stub is
// deliberately taglib-free so Phase A compiles on every host.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "core/ports/ITagReader.h"

namespace shanty {

/// Phase A stub for the concrete taglib reader. See docs/acceptance
/// trace: replaced by the real taglib implementation in SH-1001
/// Phase B (the header gains the taglib include surface there).
class TagLibReader : public ITagReader {
public:
    TagLibReader() = default;

    // STUB: always the clean miss. Phase B reads title/artist/album/
    // genre from ID3v2 / Xiph comments and durationSeconds from the
    // audio properties.
    TagMetadata read(const std::string& path) override;

private:
    TagLibReader(const TagLibReader&) = delete;
    TagLibReader& operator=(const TagLibReader&) = delete;
};

}  // namespace shanty
