// SH-603 test double: scriptable FakePlaylistCodec implementing the
// IPlaylistCodec port. The fake is trusted (its SH-301..303 counterpart is
// covered by tests/m3u_*); what this suite asserts is that PlaylistService
// and PlaylistViewModel drive it correctly and that persisted metadata
// (title/duration, SH-303 foundation) round trips through it.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/IPlaylistCodec.h"

namespace shanty {

struct FakePlaylistCodec : IPlaylistCodec {
    /// Preset outcome returned by the next parse() call.
    ParseResult nextParse;
    /// Preset outcome returned by the next serialize() call.
    SerializeResult nextSerialize;

    // ---- call log ----------------------------------------------------------
    std::vector<std::string> parseData;
    std::vector<std::string> parseFilePaths;
    std::vector<std::string> serializeTargets;
    /// Copies of the playlists handed to serialize() (metadata round trip).
    std::vector<Playlist> serializePlaylists;

    ParseResult parse(const std::string& data,
                      const std::string& filePath) override {
        parseData.push_back(data);
        parseFilePaths.push_back(filePath);
        return nextParse;
    }

    SerializeResult serialize(const Playlist& playlist,
                              const std::string& targetFilePath) override {
        serializePlaylists.push_back(playlist);
        serializeTargets.push_back(targetFilePath);
        return nextSerialize;
    }
};

}  // namespace shanty
