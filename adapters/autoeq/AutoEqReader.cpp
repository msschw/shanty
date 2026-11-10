// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later

// SH-503 Phase B — AutoEq graphic-EQ reader (implementation of the
// IAutoEqReader port, tickets §SH-503). Plain C++, no Qt (D1/D5).
//
// ===========================================================================
// PARSED-IN FORMAT DECISION
// Investigated 2026-11-10 against the real AutoEq project
// (https://github.com/jaakkopasanen/AutoEq — README, autoeq/frequency_response.py
//  [eqapo_graphic_eq() / write_eqapo_graphic_eq()], autoeq/csv.py, and
// shipped results, e.g. "results/oratory1990/over-ear/Sennheiser HD 800/"):
//
//   Findings:
//   1) "GraphicEQ.txt" — the shareable graphic-EQ export players import —
//      is exactly ONE EqualizerAPO line:
//          "GraphicEQ: 20 -0.2; 21 -0.2; ...; 19871 -15.7"
//      ~127 log-spaced "<freq> <gain>" pairs, gains with one decimal,
//      frequencies strictly ascending.
//   2) "FixedBandEQ.txt": "Preamp: -7.5 dB" plus "Filter N: ON PK Fc f Hz
//      Gain g dB Q 1.41" (10 octave bands). Investigated, NOT selected for
//      SH-503 (different shape from the ticket's canonical pair lines);
//      it can be added later behind the same port (Open/Closed).
//   3) "<model>.csv": measurement columns "frequency,raw,equalization,…"
//      — a frequency-response export, not an EQ profile.
//
//   Chosen target subset for SH-503 (same content model, two spellings):
//     (a) the canonical AutoEq "GraphicEQ: f g; f g; ..." line (one or
//         more such lines — pairs accumulate in file order);
//     (b) two-column "frequency,gain" CSV lines (the ticket's canonical
//         example, e.g. "20,6.0"), frequencies strictly ascending.
//         Tolerances: an optional "frequency,gain" header row (the first
//         non-numeric two-column line before any data pair) is skipped;
//         an optional "Preamp: <x> dB" line is recorded in the result but
//         never applied to the gains (EqualizerProfile has no preamp
//         field; global-trim handling is a follow-up decision).
//   Everything else yields a structured AutoEqError (value, no
//   exceptions): UnrecognizedFormat for prose / non-subset files
//   (fixed-band or parametric "Filter ..." exports, measurement CSVs),
//   BadField for malformed or non-numeric fields, NoData for empty input.
//
// - FREQUENCY→BAND MAPPING: bands are the file's own frequencies in
//   ascending order; gains() are the dB gains in that same order. The
//   reader does NOT resample into a fixed band grid (Shanty's device EQ
//   band layout) — that resampler is out-of-scope, follow-up for a
//   future ticket.
// - NUMERIC TOLERANCE: decimal text → double via std::strtod (default
//   process locale, decimal point "."), full-field consumption for pairs
//   (first-token consumption for the preamp value, units follow).
//   Whitespace around fields, CRLF/LF endings, blank lines and one
//   leading UTF-8 BOM are tolerated. Values are kept exactly as read —
//   no rounding, rescaling or clamping (out-of-range gains simply make
//   the profile invalid; see AutoEqParseResult.h and the SH-503 suite).
// - Whiteleniency note: stray content before a "GraphicEQ:" marker line
//   other than Preamp/blank lines is ignored (AutoEq files are marker +
//   one line; being lenient costs nothing).
// ===========================================================================

#include "AutoEqReader.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace shanty {

namespace {

constexpr const char* kUtf8Bom = "\xEF\xBB\xBF";
constexpr const char* kGraphicEqMarker = "GraphicEQ:";
constexpr const char* kPreampMarker = "Preamp:";

bool isSpace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ||
           ch == '\f' || ch == '\v';
}

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && isSpace(text[begin])) {
        ++begin;
    }
    while (end > begin && isSpace(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool startsWithIgnoreCase(const std::string& text, const char* marker) {
    const std::size_t length = std::strlen(marker);
    if (text.size() < length) {
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) !=
            std::tolower(static_cast<unsigned char>(marker[i]))) {
            return false;
        }
    }
    return true;
}

/// Decimal text → double with FULL-CONSUMPTION; rejects empty fields,
/// trailing junk ("abc", "1.5 dB") and non-finite values.
bool parseNumber(const std::string& text, double* out) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.c_str();
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin || *end != '\0') {
        return false;
    }
    if (!std::isfinite(value)) {
        return false;  // "inf"/"nan" are never meaningful EQ gains/frequencies
    }
    *out = value;
    return true;
}

AutoEqError makeError(AutoEqError::Code code, const std::string& message,
                      int line) {
    AutoEqError error;
    error.code = code;
    error.message = message;
    error.line = line;
    return error;
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : text) {
        if (ch == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

std::vector<std::string> splitWhitespace(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (isSpace(ch)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

/// A parsed "frequency gain" pair together with its 1-based source line
/// (for one-line formats every pair shares the marker's line number).
struct Pair {
    double frequency = 0.0;
    double gainDb = 0.0;
    int line = 0;
};

/// Records a "Preamp: <x> dB" line. Returns false on an instance where the
/// value is non-numeric (caller turns that into a BadField error).
bool parsePreampLine(const std::string& content, AutoEqParseResult* result) {
    const std::string valueText = trim(content.substr(std::strlen(kPreampMarker)));
    if (valueText.empty()) {
        return false;
    }
    const char* begin = valueText.c_str();
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    // First-token consumption: a trailing unit ("dB") is fine; no number
    // at all / unparseable value is a BadField case instead.
    if (end == begin || !std::isfinite(value)) {
        return false;
    }
    result->hasPreamp = true;
    result->preampDb = value;
    return true;
}

/// Parses one "GraphicEQ: ..." line body into pairs.
void parseGraphicEqBody(const std::string& body, int line,
                        std::vector<Pair>* pairs,
                        std::vector<AutoEqError>* errors) {
    const std::vector<std::string> chunks = split(body, ';');
    for (const std::string& chunk : chunks) {
        const std::vector<std::string> tokens = splitWhitespace(trim(chunk));
        Pair pair;
        if (tokens.size() != 2 ||
            !parseNumber(tokens[0], &pair.frequency) ||
            !parseNumber(tokens[1], &pair.gainDb)) {
            errors->push_back(makeError(
                AutoEqError::Code::BadField,
                "malformed \"frequency gain\" pair in GraphicEQ output "
                "(expected: \"<freq> <gain>\")",
                line));
            return;  // stop at first malformed pair: no partial data
        }
        pair.line = line;
        pairs->push_back(pair);
    }
}

}  // namespace

AutoEqParseResult AutoEqReader::read(const std::string& data,
                                     const std::string& filePath) {
    (void)filePath;  // metadata only; the application names the profile
    AutoEqParseResult result;

    std::string content = data;
    // Tolerance: one UTF-8 BOM at the very start is stripped.
    if (content.rfind(kUtf8Bom, 0) == 0) {
        content.erase(0, std::strlen(kUtf8Bom));
    }

    // Split into trimmed lines, keeping 1-based numbering (CRLF handled by
    // trimming the '\r' with every line).
    std::vector<std::string> stripped;
    std::vector<int> lineNumbers;
    {
        std::vector<std::string> raw = split(content, '\n');
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const std::string line = trim(raw[i]);
            if (line.empty()) {
                continue;
            }
            stripped.push_back(line);
            lineNumbers.push_back(static_cast<int>(i) + 1);
        }
    }

    // Format selection: a "GraphicEQ:" marker selects the EqualizerAPO
    // spelling; anything else is attempted as "frequency,gain" CSV.
    int markerIndex = -1;
    for (std::size_t i = 0; i < stripped.size(); ++i) {
        if (startsWithIgnoreCase(stripped[i], kGraphicEqMarker)) {
            markerIndex = static_cast<int>(i);
            break;
        }
    }

    std::vector<Pair> pairs;
    bool headerSkipped = false;
    bool sawAnyCandidate = false;  // non-blank, non-preamp line seen

    if (markerIndex >= 0) {
        // GraphicEQ spelling. Preamp lines before the marker are recorded.
        for (int i = 0; i < markerIndex; ++i) {
            if (startsWithIgnoreCase(stripped[i], kPreampMarker) &&
                !parsePreampLine(stripped[i], &result)) {
                result.errors.push_back(
                    makeError(AutoEqError::Code::BadField,
                              "Preamp line has no numeric value",
                              lineNumbers[i]));
            }
        }
        for (int i = markerIndex; i < static_cast<int>(stripped.size()); ++i) {
            if (!startsWithIgnoreCase(stripped[i], kGraphicEqMarker)) {
                if (!startsWithIgnoreCase(stripped[i], kPreampMarker)) {
                    continue;  // documented leniency: stray filler lines
                }
                if (!parsePreampLine(stripped[i], &result)) {
                    result.errors.push_back(
                        makeError(AutoEqError::Code::BadField,
                                  "Preamp line has no numeric value",
                                  lineNumbers[i]));
                }
                continue;
            }
            parseGraphicEqBody(stripped[i].substr(std::strlen(kGraphicEqMarker)),
                               lineNumbers[i], &pairs, &result.errors);
        }
    } else {
        // CSV spelling: "frequency,gain" pairs (ascending), optional header
        // row and optional Preamp lines.
        for (std::size_t i = 0; i < stripped.size(); ++i) {
            if (startsWithIgnoreCase(stripped[i], kPreampMarker)) {
                if (!parsePreampLine(stripped[i], &result)) {
                    result.errors.push_back(
                        makeError(AutoEqError::Code::BadField,
                                  "Preamp line has no numeric value",
                                  lineNumbers[i]));
                }
                continue;
            }
            sawAnyCandidate = true;
            if (stripped[i].find(',') == std::string::npos) {
                result.errors.push_back(makeError(
                    AutoEqError::Code::UnrecognizedFormat,
                    "no \"GraphicEQ:\" marker and not a "
                    "\"frequency,gain\" line",
                    lineNumbers[i]));
                break;  // stop at the first non-subset line: no partial data
            }
            const std::vector<std::string> fields =
                split(stripped[i], ',');
            if (fields.size() != 2) {
                result.errors.push_back(makeError(
                    AutoEqError::Code::BadField,
                    "expected exactly two \"frequency,gain\" columns",
                    lineNumbers[i]));
                break;
            }
            const std::string frequencyField = trim(fields[0]);
            const std::string gainField = trim(fields[1]);
            Pair pair;
            if (parseNumber(frequencyField, &pair.frequency) &&
                parseNumber(gainField, &pair.gainDb)) {
                pair.line = lineNumbers[i];
                pairs.push_back(pair);
                continue;
            }
            if (pairs.empty() && !headerSkipped) {
                // Tolerance: one optional non-numeric header row
                // (e.g. "frequency,gain") before any data pair.
                headerSkipped = true;
                continue;
            }
            result.errors.push_back(makeError(
                AutoEqError::Code::BadField,
                "non-numeric or malformed \"frequency,gain\" field",
                lineNumbers[i]));
            break;
        }
    }

    if (result.errors.empty() && pairs.empty()) {
        // Cover both empty input and "only removable lines" (blank,
        // header, preamp-only) — one clean structured error either way.
        result.errors.push_back(makeError(
            AutoEqError::Code::NoData,
            sawAnyCandidate
                ? "no data lines (only blank/header/Preamp lines)"
                : "input is empty",
            0));
    }

    // Band ordering contract: frequencies must be strictly ascending; a
    // duplicate or descending pair breaks the file-order == gains-order
    // band mapping, so it is a structured error (no implicit sort).
    if (result.errors.empty()) {
        for (std::size_t i = 1; i < pairs.size(); ++i) {
            if (!(pairs[i].frequency > pairs[i - 1].frequency)) {
                result.errors.push_back(makeError(
                    AutoEqError::Code::FrequencyNotAscending,
                    "frequencies must be strictly ascending "
                    "(duplicate or descending pair)",
                    pairs[i].line));
                break;
            }
        }
    }

    // Result assembly: on any error no partial profile is exposed;
    // out-of-range gains are NOT errors — the profile is built and its
    // own isValid() is false (downstream save checks reject it; gain
    // normalization is a follow-up decision, see AutoEqParseResult.h).
    if (!result.errors.empty()) {
        return result;
    }

    std::vector<double> gains;
    gains.reserve(pairs.size());
    for (const Pair& pair : pairs) {
        gains.push_back(pair.gainDb);
    }
    // Name decision: the reader leaves the name empty; the application
    // assigns the user-facing name at import time (e.g. from the file
    // name). Source: Imported (SH-503 source tracking).
    result.profile = EqualizerProfile::makeWithSource(
        std::string(), gains, ProfileSource::Imported);
    return result;
}

}  // namespace shanty
