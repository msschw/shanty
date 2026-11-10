// SH-301..303 adapter: M3U / m3u8 codec (IPlaylistCodec).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "core/ports/IPlaylistCodec.h"

namespace shanty {

class M3uPlaylistCodec : public IPlaylistCodec {
public:
    ParseResult parse(const std::string& data,
                      const std::string& filePath) override;

    SerializeResult serialize(const Playlist& playlist,
                              const std::string& targetFilePath) override;
};

}  // namespace shanty
