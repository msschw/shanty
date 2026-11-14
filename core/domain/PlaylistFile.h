// SH-1002 (SH-E10 UX-13): playlist file classification. Owner decision
// (2026-11-14): a playlist = an .m3u/.m3u8 file found within the
// library — the same indexed-file question AudioFormat answers for
// audio, so it gets the same single-source-of-truth treatment in the
// domain (kept OUT of AudioFormat.h, whose charter is *playable audio*).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace shanty {

/// True if the file path ends with a supported playlist extension
/// (.m3u, .m3u8 — case-insensitive). Stub in Phase A: always false so
/// the red-first suites fail by assertion; SH-1002 Phase B implements.
bool isSupportedPlaylistPath(const std::string& path);

}  // namespace shanty
