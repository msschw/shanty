// SH-1009 (UX-20): artwork-reading port. Embedded cover art is read-only
// presentation data: the domain/catalogue layer decides WHICH track's
// picture represents a row, the adapter knows HOW to extract it from the
// container. A separate port (not an ITagReader method) keeps the tag
// metadata path (small strings, read for every track at scan time) apart
// from artwork bytes (larger, read lazily only for rows being shown).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace shanty {

/// One file's embedded artwork. A **clean miss** — no picture, absent,
/// unreadable or unsupported file — is an empty Artwork (both fields
/// empty); implementations MUST NOT throw.
struct Artwork {
    /// The picture's MIME type as tagged (e.g. "image/jpeg"); may be
    /// empty when the tag omits it (the consumer sniffs the data). Empty
    /// on a clean miss.
    std::string mimeType;
    /// The encoded image bytes (PNG/JPEG/...). Empty on a clean miss.
    std::string data;

    bool isEmpty() const { return data.empty(); }
};

class IArtworkReader {
public:
    virtual ~IArtworkReader() = default;

    /// The file's preferred embedded picture (front cover when the
    /// container distinguishes types; otherwise the first picture), or
    /// the empty Artwork on a clean miss.
    virtual Artwork read(const std::string& path) = 0;
};

}  // namespace shanty
