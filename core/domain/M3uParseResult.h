// SH-301: result of parsing a playlist. Errors are values, not exceptions
// (see ISSUES.md SH-301: "no exceptions as control flow on the happy path").
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "Playlist.h"

namespace shanty {

struct ParseError {
    enum class Code {
        Unknown,             // placeholder before implementation
        NotAPlaylist,        // file lacks any playlist structure
        UndecodableEncoding, // payload is neither UTF-8 nor Latin-1
        Internal             // codec bug guard
    };

    Code code = Code::Unknown;
    std::string message;
    /// 1-based line number of the offending input when known, else 0.
    int line = 0;
};

struct ParseResult {
    Playlist playlist;
    std::vector<ParseError> errors;

    bool ok() const { return errors.empty(); }
};

struct SerializeResult {
    enum class Status { Success, Error };

    Status status = Status::Success;
    std::string data;
    std::string errorMessage;
};

}  // namespace shanty
