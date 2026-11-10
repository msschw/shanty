// SH-301 port (Dependency Inversion, open/closed for future formats).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "../domain/M3uParseResult.h"

namespace shanty {

class IPlaylistCodec {
public:
    virtual ~IPlaylistCodec() = default;

    /// Reads playlist bytes. `filePath` carries the file's extension, which
    /// selects the encoding rules (.m3u vs .m3u8 → SH-302).
    virtual ParseResult parse(const std::string& data,
                              const std::string& filePath) = 0;

    /// Serializes the playlist in M3U format. `targetFilePath` decides
    /// the encoding (see SH-303).
    virtual SerializeResult serialize(const Playlist& playlist,
                                      const std::string& targetFilePath) = 0;
};

}  // namespace shanty
