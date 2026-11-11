// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// M3U / M3U8 codec (SH-301 parse, SH-302 encoding rules, SH-303 serialize).
//
// Storage convention (unicode-preserving, per the SH-302 Phase-A contract):
// metadata strings in PlaylistEntry are ALWAYS the UTF-8 encoding of the
// decoded code points, so `QString::fromStdString(title)` yields the true
// Unicode string. For a valid-UTF-8 payload this is byte-identical to the
// input; for a Latin-1-decoded payload each decoded code point is re-encoded
// to UTF-8 on store (e.g. metadata bytes "4D F6 64 65" -> stored
// "4D C3 B6 64 65"). Paths follow the same rule (D1: the domain deals in
// plain UTF-8 paths).
//
// Scanning is byte-oriented and Qt-free; only encoding *validation*/export
// transcoding uses the Qt-free UTF-8 validator below (adapters stays Qt-free).
//
// Parse rules (SH-301/302):
//  - "#EXTM3U" header is optional.
//  - Blank / whitespace-only lines and comment lines are skipped, except
//    "#EXTINF:<duration>,<title>" which carries metadata for the next entry.
//    Other '#' lines (incl. the VLC playlist-title extension) are comments.
//  - CRLF and CR line endings are normalized to LF.
//  - A non-comment, non-blank line is an entry line: it must contain no
//    whitespace. Entry lines with whitespace (or any other garbage) are
//    reported as ParseError::NotAPlaylist with their 1-based line number;
//    a document containing such a line does not produce an ok() result.
//  - Empty input (or input with no entry lines and no garbage) parses to an
//    empty, ok() playlist.
//
// Encoding rules (SH-302):
//  - .m3u8: the payload must be valid UTF-8 (checked with a strict RFC 3629
//    validator). If it is not, no Latin-1 fallback is attempted — the caller
//    gets ParseError::UndecodableEncoding instead of mojibake.
//  - .m3u: a UTF-8 BOM is sniffed first; when present the payload decodes as
//    UTF-8 (same strictness as .m3u8). Without BOM the metadata decodes as
//    Latin-1, which is total (every byte maps), so no UndecodableEncoding
//    can occur on that path.
//  - Unknown extensions sniff for the BOM; with BOM → UTF-8 rules, else the
//    permissive Latin-1 rules.
//  - "Decodability" test: Qt's UTF-8 validity check semantics are mirrored
//    by decodeUtf8Points(); Latin-1 never fails, so neither-decodes-sense
//    only happens for a UTF-8-declared payload that is not valid UTF-8.

#include <climits>
#include <cstdlib>

#include "M3uPlaylistCodec.h"

#include <cctype>
#include <utility>
#include <vector>

namespace shanty {
namespace {

const char kBom[] = "\xEF\xBB\xBF";
const char kExtInfPrefix[] = "#EXTINF:";
const char kExtM3uHeader[] = "#EXTM3U";
const char kUnknownDoc[] = "unknown";

bool hasUtf8Bom(const std::string& data) {
    return data.size() >= 3 && data.compare(0, 3, kBom, 3) == 0;
}

/// Strict UTF-8 decode (RFC 3629, mirroring Qt's UTF-8 decoder acceptance).
/// Returns false when the input is not canonically decodable UTF-8.
bool decodeUtf8Points(const std::string& s, std::vector<unsigned int>& points) {
    points.clear();
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    const std::size_t n = s.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char b = p[i];
        if (b < 0x80) {
            points.push_back(b);
            ++i;
            continue;
        }
        std::size_t len;
        if (b >= 0xC2 && b <= 0xDF) len = 2;        // C0/C1 rejected (overlong)
        else if (b >= 0xE0 && b <= 0xEF) len = 3;
        else if (b >= 0xF0 && b <= 0xF4) len = 4;
        else return false;                          // invalid lead (incl. F5..FF)
        if (i + len > n) return false;
        // Second byte carries the lower bound for E0/F0 ranges to reject
        // overlong encodings; ED/F4 upper bounds reject surrogates/>U+10FFFF.
        const unsigned char b1 = p[i + 1];
        if ((b1 & 0xC0) != 0x80) return false;
        if (b == 0xE0 && b1 < 0xA0) return false;
        if (b == 0xED && b1 > 0x9F) return false;
        if (b == 0xF0 && b1 < 0x90) return false;
        if (b == 0xF4 && b1 > 0x8F) return false;
        // Wave-8b fix (locked by tst_m3u_roundtrip): the code point must
        // assemble from the lead byte's payload bits too — the previous
        // `cp = b1 & 0x3F` start dropped them, so every multi-byte sequence
        // decoded to garbage (2-byte "C3 A9" → U+0029) and the .m3u
        // Latin-1 export silently corrupted titles (device-observed:
        // exported control bytes 03/19 after a Latin-1 round trip).
        unsigned int cp =
            (b & (len == 2 ? 0x1Fu : len == 3 ? 0x0Fu : 0x07u)) << 6;
        cp |= b1 & 0x3F;
        if (len >= 3) {
            if ((p[i + 2] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i + 2] & 0x3F);
        }
        if (len == 4) {
            if ((p[i + 3] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i + 3] & 0x3F);
        }
        points.push_back(cp);
        i += len;
    }
    return true;
}

bool validUtf8(const std::string& s) {
    std::vector<unsigned int> ignored;
    return decodeUtf8Points(s, ignored);
}

/// Latin-1 bytes -> UTF-8 bytes (U+0000..U+00FF per byte).
std::string latin1ToUtf8(const std::string& bytes) {
    std::string out;
    out.reserve(bytes.size());
    for (unsigned char b : bytes) {
        if (b < 0x80) {
            out.push_back(static_cast<char>(b));
        } else {
            out.push_back(static_cast<char>(0xC0 | (b >> 6)));
            out.push_back(static_cast<char>(0x80 | (b & 0x3F)));
        }
    }
    return out;
}

/// UTF-8 bytes -> Latin-1 bytes when every code point fits in U+00FF
/// (and the input is canonically decodable UTF-8); std::nullopt otherwise.
bool utf8ToLatin1IfPossible(const std::string& bytes, std::string& latin1) {
    std::vector<unsigned int> points;
    if (!decodeUtf8Points(bytes, points)) return false;
    latin1.clear();
    latin1.reserve(points.size());
    for (unsigned int cp : points) {
        if (cp > 0xFF) return false;
        latin1.push_back(static_cast<char>(cp));
    }
    return true;
}

/// Splits payload into LF-terminated lines; CRs at line end are normalized
/// away (CRLF and legacy CR). Applies after any BOM was stripped.
std::vector<std::string> splitLines(const std::string& payload) {
    std::vector<std::string> lines;
    std::string line;
    for (char c : payload) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            if (c != '\r') line.push_back(c);
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

bool isBlankLine(const std::string& line) {
    for (char c : line)
        if (c != ' ' && c != '\t' && c != '\v' && c != '\f') return false;
    return true;
}

bool hasInternalWhitespace(const std::string& line) {
    for (char c : line)
        if (c == ' ' || c == '\t' || c == '\v' || c == '\f') return true;
    return false;
}

/// Extracts duration (kUnknownDuration when absent/non-numeric) and the raw
/// title suffix from an "#EXTINF:" metadata line. The title is still in the
/// payload encoding; the caller re-encodes it per the storage convention.
void parseExtInf(const std::string& line, int& duration, std::string& title) {
    duration = Track::kUnknownDuration;
    title.clear();
    const std::string rest = line.substr(sizeof(kExtInfPrefix) - 1);
    const std::size_t comma = rest.find(',');
    const std::string durationField =
        (comma == std::string::npos) ? rest : rest.substr(0, comma);
    if (!durationField.empty()) {
        char* end = nullptr;
        const long long value = std::strtoll(durationField.c_str(), &end, 10);
        if (end != durationField.c_str()) {  // at least one digit parsed
            if (value > INT_MAX) duration = INT_MAX;
            else if (value < INT_MIN) duration = INT_MIN;
            else duration = static_cast<int>(value);
        }
    }
    if (comma != std::string::npos) title = rest.substr(comma + 1);
}

}  // namespace

ParseResult M3uPlaylistCodec::parse(const std::string& data,
                                    const std::string& filePath) {
    ParseResult result;

    // --- SH-302: select the decode rules from extension + BOM sniffing.
    std::string extension = kUnknownDoc;
    const std::size_t dot = filePath.rfind('.');
    if (dot != std::string::npos) {
        extension = filePath.substr(dot);
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const bool declaresUtf8 = (extension == ".m3u8");
    const bool hasBom = hasUtf8Bom(data);
    const std::string body = hasBom ? data.substr(3) : data;
    const bool utf8Payload = declaresUtf8 || hasBom;

    if (utf8Payload && !validUtf8(body)) {
        // Neither encoding "makes sense": a UTF-8-declared payload that is
        // not valid UTF-8. Latin-1 is never attempted here to keep error
        // handling explicit and mojibake impossible.
        ParseError error;
        error.code = ParseError::Code::UndecodableEncoding;
        error.message = "payload is declared UTF-8 but is not valid UTF-8";
        error.line = 0;
        result.errors.push_back(error);
        return result;
    }

    // --- SH-301: line-oriented scan.
    std::string extInfTitle;
    int extInfDuration = Track::kUnknownDuration;
    bool havePendingMetadata = false;

    int lineNumber = 0;
    for (const std::string& rawLine : splitLines(body)) {
        ++lineNumber;
        if (isBlankLine(rawLine)) continue;
        if (rawLine.compare(0, 1, "#") == 0) {
            if (rawLine.compare(0, sizeof(kExtInfPrefix) - 1, kExtInfPrefix) == 0) {
                parseExtInf(rawLine, extInfDuration, extInfTitle);
                havePendingMetadata = true;
            }
            continue;  // any other '#' line is a comment
        }
        if (rawLine.compare(0, sizeof(kExtM3uHeader) - 1, kExtM3uHeader) == 0)
            continue;  // "#EXTM3U" header itself (tolerated also when absent)

        if (hasInternalWhitespace(rawLine)) {
            // Not a playlist: entry lines must be a single path/URL token.
            ParseError error;
            error.code = ParseError::Code::NotAPlaylist;
            error.message = "entry lines must be single tokens, found: line "
                            + std::to_string(lineNumber);
            error.line = lineNumber;
            result.errors.push_back(error);
            continue;
        }

        // Storage convention: always UTF-8. A Latin-1-decoded payload
        // re-encodes every decoded code point to UTF-8 on store; a valid
        // UTF-8 payload stores its (already UTF-8) bytes unchanged.
        PlaylistEntry entry;
        entry.path = utf8Payload ? rawLine : latin1ToUtf8(rawLine);
        if (havePendingMetadata) {
            entry.title =
                utf8Payload ? extInfTitle : latin1ToUtf8(extInfTitle);
            entry.durationSeconds = extInfDuration;
            havePendingMetadata = false;
            extInfTitle.clear();
            extInfDuration = Track::kUnknownDuration;
        }
        result.playlist.add(entry);
    }

    return result;
}

SerializeResult M3uPlaylistCodec::serialize(const Playlist& playlist,
                                            const std::string& targetFilePath) {
    std::string extension = kUnknownDoc;
    const std::size_t dot = targetFilePath.rfind('.');
    if (dot != std::string::npos) {
        extension = targetFilePath.substr(dot);
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const bool utf8Target = (extension == ".m3u8");

    // --- SH-303 export encoding decision (documented per ticket).
    // Stored metadata is always UTF-8 (see storage convention above).
    //  - ".m3u8" target: emitted as UTF-8, i.e. stored bytes pass through
    //    verbatim. (Defensive case only: a Playlist built outside this codec
    //    may hold non-UTF-8 title bytes; those are transcoded Latin-1 -> UTF-8.)
    //  - ".m3u" target (Latin-1 export): stored UTF-8 metadata is re-encoded
    //    to Latin-1 whenever every decoded code point fits in U+00FF, so a
    //    ".m3u" file carries real Latin-1 and re-parses to the same Unicode.
    //    Titles with code points beyond Latin-1 cannot be represented in a
    //    Latin-1 ".m3u"; their bytes are then emitted unchanged (UTF-8 bytes
    //    in a nominally Latin-1 file — Shanty-internal round trips stay
    //    lossless; strict interop should export via ".m3u8"). This is the
    //    choice recorded for ticket SH-303; tests only require both targets
    //    to succeed and to round-trip through ".m3u8" parsing.

    std::string out;
    out += kExtM3uHeader;
    out += '\n';

    for (const PlaylistEntry& entry : playlist.entries()) {
        std::string titleBytes = entry.title;
        std::string pathBytes = entry.path;
        if (utf8Target) {
            if (!validUtf8(titleBytes)) titleBytes = latin1ToUtf8(titleBytes);
            if (!validUtf8(pathBytes)) pathBytes = latin1ToUtf8(pathBytes);
        } else {
            std::string encoded;
            if (utf8ToLatin1IfPossible(titleBytes, encoded)) titleBytes = encoded;
            if (utf8ToLatin1IfPossible(pathBytes, encoded)) pathBytes = encoded;
        }

        // Corruption guard: a CR/LF inside path or title would inject extra
        // M3U lines. Reported as an error, not silently rewritten.
        const bool badPath = pathBytes.find('\n') != std::string::npos ||
                             pathBytes.find('\r') != std::string::npos;
        const bool badTitle = titleBytes.find('\n') != std::string::npos ||
                              titleBytes.find('\r') != std::string::npos;
        if (badPath || badTitle) {
            SerializeResult error;
            error.status = SerializeResult::Status::Error;
            error.errorMessage =
                "entry path/title contains a line break and cannot be serialized";
            return error;
        }

        // "#EXTINF:<dur>,<title>" only when metadata exists: duration known,
        // or a title present (then duration is printed as stored, which is
        // kUnknownDuration == -1 and re-parses back to "unknown").
        if (entry.durationSeconds != Track::kUnknownDuration || !titleBytes.empty()) {
            out += kExtInfPrefix;
            out += std::to_string(entry.durationSeconds);
            out += ',';
            out += titleBytes;
            out += '\n';
        }
        out += pathBytes;
        out += '\n';
    }

    SerializeResult result;
    result.data = std::move(out);
    result.status = SerializeResult::Status::Success;
    return result;
}

}  // namespace shanty
