// SH-1001: tag metadata value type (SH-E10 UX-12 — read-only tag data).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace shanty {

/// Tag metadata read from an audio file. Empty strings mean "no such
/// tag" (there is no separate "unset" state — tags are read-only data,
/// never authored by this app). durationSeconds uses the same unknown
/// marker as Track::kUnknownDuration (-1).
///
/// A fully-empty TagMetadata is the documented **clean miss**: readers
/// never throw, so an absent, unreadable or unsupported file yields an
/// empty value and the caller decides the fallbacks (e.g. title =
/// basename, duration stays unknown).
struct TagMetadata {
    static const int kUnknownDuration = -1;

    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int durationSeconds = kUnknownDuration;

    bool isEmpty() const {
        return title.empty() && artist.empty() && album.empty()
            && genre.empty() && durationSeconds == kUnknownDuration;
    }
};

}  // namespace shanty
