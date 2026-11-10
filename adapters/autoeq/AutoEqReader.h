// SH-503: adapter implementation of the IAutoEqReader port for AutoEq
// graphic-EQ profile files. Deliberately Qt-free (plain C++ parser; the
// deployment-side "file → bytes" step stays in the application layer), so
// it is host-testable and the adapters static lib needs no extra qt
// entries. The parsed-in format subset (and the investigated
// alternatives) is documented as a decision record at the top of
// AutoEqReader.cpp.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "core/ports/IAutoEqReader.h"

namespace shanty {

class AutoEqReader : public IAutoEqReader {
public:
    // SH-503 Phase A stub: always reports an internal "unimplemented"
    // error; Phase B replaces read() with the parser specified in the
    // decision record at the top of AutoEqReader.cpp.
    AutoEqParseResult read(const std::string& data,
                           const std::string& filePath) override;
};

}  // namespace shanty
