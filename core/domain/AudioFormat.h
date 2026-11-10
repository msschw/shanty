// SH-201 (SH-403): supported audio formats. Extension classification is the
// single source of truth for "which audio files can be played".
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace shanty {

enum class AudioFormat { MP3, OGG, FLAC, Unsupported };

/// Classifies a file *extension* (case-insensitive, leading dot optional).
AudioFormat classifyExtension(const std::string& extension);

/// True if the file path ends with a supported audio extension
/// (mp3, ogg, flac).
bool isSupportedAudioPath(const std::string& path);

}  // namespace shanty
