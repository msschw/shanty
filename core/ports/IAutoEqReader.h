// SH-503 port (Dependency Inversion, ISSUES.md §1.1/§1.2). Design decision:
// AutoEq profile import goes through a port + adapter instead of a single
// core-domain reader — like IPlaylistCodec/ILibraryScanner — so concrete
// AutoEq export spellings (graphic-EQ line format today; fixed-band or
// parametric files later, see the decision record in AutoEqReader.cpp) can
// be added without touching the domain or the UI (Open/Closed; D5:
// AutoEq profiles are data behind an abstraction).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "../domain/AutoEqParseResult.h"

namespace shanty {

class IAutoEqReader {
public:
    virtual ~IAutoEqReader() = default;

    /// Parses an AutoEq profile from `data` (the file content). `filePath`
    /// is metadata only (same shape as IPlaylistCodec::parse), no I/O is
    /// performed. Returns a value result: errors live in
    /// AutoEqParseResult::errors (ok() == errors.empty()), never exceptions.
    virtual AutoEqParseResult read(const std::string& data,
                                   const std::string& filePath
                                       = std::string()) = 0;
};

}  // namespace shanty
