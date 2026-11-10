// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "M3uPlaylistCodec.h"

namespace shanty {
// TODO(SH-301/302/303 Phase B): implement parsing, encoding rules and
// serialization per the ticket test lists. Behaviour notes:
//  - tolerate missing #EXTM3U header, skip comment lines except EXTINF,
//  - .m3u8 decodes as UTF-8, .m3u falls back to Latin-1 (with BOM sniffing),
//  - round-trip preserves entries and metadata semantically.

ParseResult M3uPlaylistCodec::parse(const std::string&, const std::string&) {
    ParseResult result;  // unimplemented: empty result (tests must fail red)
    result.errors.push_back(ParseError{ParseError::Code::Internal,
                                       "not implemented", 0});
    return result;
}

SerializeResult M3uPlaylistCodec::serialize(const Playlist&,
                                            const std::string&) {
    SerializeResult result;  // unimplemented (tests must fail red)
    result.status = SerializeResult::Status::Error;
    result.errorMessage = "not implemented";
    return result;
}
}  // namespace shanty
