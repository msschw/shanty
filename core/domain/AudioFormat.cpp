// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioFormat.h"

#include <cctype>

namespace shanty {
namespace {

/// Lower-cases an ASCII string (paths are UTF-8; extension suffixes to match
/// are ASCII, so byte-wise lowering is sufficient).
std::string toLowerAscii(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        result.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

}  // namespace

AudioFormat classifyExtension(const std::string& extension) {
    if (extension.empty()) {
        return AudioFormat::Unsupported;
    }
    // Leading dot is optional ("mp3" and ".mp3" are equivalent).
    std::string name = extension;
    if (name.front() == '.') {
        name.erase(name.begin());
    }
    if (name.empty()) {
        return AudioFormat::Unsupported;
    }
    name = toLowerAscii(name);
    if (name == "mp3") {
        return AudioFormat::MP3;
    }
    if (name == "ogg") {
        return AudioFormat::OGG;
    }
    if (name == "flac") {
        return AudioFormat::FLAC;
    }
    return AudioFormat::Unsupported;
}

bool isSupportedAudioPath(const std::string& path) {
    // The *final* extension decides: "/music/song.v2.mp3" is an MP3.
    const std::size_t lastDot = path.find_last_of('.');
    if (lastDot == std::string::npos || lastDot + 1 >= path.size()) {
        return false;
    }
    const std::string extension = path.substr(lastDot + 1);
    // A dot inside a directory component is not an extension
    // ("/music.v2/song" has no extension).
    if (extension.find('/') != std::string::npos) {
        return false;
    }
    return classifyExtension(extension) != AudioFormat::Unsupported;
}

}  // namespace shanty
