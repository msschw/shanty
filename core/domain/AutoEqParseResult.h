// SH-503: result of parsing an AutoEq profile file. Errors are values, not
// exceptions — mirrors the core/domain/M3uParseResult.h pattern (see
// ISSUES.md SH-301: "no exceptions as control flow on the happy path").
// The parsed data lands in an EqualizerProfile (SH-501 domain type,
// architectural decision D5: AutoEq profiles are data behind an
// abstraction), with SH-503's additive source tracking (ProfileSource).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "EqualizerProfile.h"

namespace shanty {

struct AutoEqError {
    enum class Code {
        Unknown,               // placeholder before implementation
        NoData,                // empty file / only blank (or removable) lines
        UnrecognizedFormat,    // content is not one of the documented
                               // AutoEq graphic-EQ spellings (see the
                               // decision record in
                               // adapters/autoeq/AutoEqReader.cpp)
        BadField,              // malformed or non-numeric field (line known)
        FrequencyNotAscending, // duplicate or descending band frequencies
        Internal               // reader bug guard
    };

    Code code = Code::Unknown;
    std::string message;
    /// 1-based line number of the offending input when known, else 0. For
    /// one-line formats (AutoEq "GraphicEQ: ..." in a single line) all
    /// offending pairs share the marker's line number.
    int line = 0;
};

struct AutoEqParseResult {
    /// The parsed profile: bands follow the file's own frequencies in
    /// ascending order; gains() are the dB gains in that order. Empty and
    /// (therefore) invalid when parse errors occurred.
    EqualizerProfile profile;

    /// Carries the input's declared preamp (finding: AutoEq files such as
    /// "Preamp: -7.5 dB" ship a global trim suggestion). EqualizerProfile
    /// has no preamp field (SH-501 contract), so this is informational
    /// for now; a global-trim feature is a follow-up decision, and the
    /// gains are parsed unchanged from the file either way.
    bool hasPreamp = false;
    double preampDb = 0.0;

    std::vector<AutoEqError> errors;

    bool ok() const { return errors.empty(); }
};

}  // namespace shanty
