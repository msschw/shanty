// SH-303 Phase A: M3U serialization and semantic round trip.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "adapters/m3u/M3uPlaylistCodec.h"
#include "domain/M3uParseResult.h"

using shanty::M3uPlaylistCodec;
using shanty::Playlist;
using shanty::PlaylistEntry;
using shanty::Track;
namespace{
using ParseResult = shanty::ParseResult;
}

namespace {

PlaylistEntry entry(const std::string& path, const std::string& title = "",
                    int duration = Track::kUnknownDuration) {
    PlaylistEntry e;
    e.path = path;
    e.title = title;
    e.durationSeconds = duration;
    return e;
}

Playlist parseOf(const std::string& data, const std::string& file) {
    M3uPlaylistCodec codec;
    ParseResult result = codec.parse(data, file);
    if (result.errors.empty()) return Playlist(result.playlist);
    return Playlist();
}

/// Line structure assertions that ignore whitespace-only differences
/// (semantic, not byte-equality, comparison per SH-303).
QStringList logicalLines(const std::string& data) {
    QStringList list;
    const QStringList raw = QString::fromStdString(data).split(
        QRegularExpression("\\r?\\n"));
    for (const QString& line : raw)
        if (!line.isEmpty()) list << line;
    return list;
}

Playlist simplePlaylist() {
    Playlist playlist;
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/b.flac", "B Song", 123));
    return playlist;
}

}  // namespace

class TestM3uRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void serializesHeaderAndEntries();
    void serializesExtInfWhenKnown();
    void roundTripKeepsEntriesAndMetadata();
    void roundTripSurvivesWhitespaceDifferences();
    void exportTargetExtensionChoosesEncodingRule();
    // Wave-8b device-drive regression: decodeUtf8Points dropped the lead
    // byte's bits, so 2-byte UTF-8 sequences (U+0080..U+07FF, e.g. é/ï)
    // decoded to a bare 6-bit value and the .m3u Latin-1 export silently
    // rewrote titles ("Café" → "Caf)"; on device the exported file even
    // carried control bytes 03/19 from the double-encoded detour).
    void latin1ExportRepresentsTwoByteCodePointsExactly();
    void latin1RoundTripIsByteLevelIdempotent();
};

void TestM3uRoundtrip::serializesHeaderAndEntries() {
    M3uPlaylistCodec codec;
    const shanty::SerializeResult result =
        codec.serialize(simplePlaylist(), "/x/playlist.m3u8");
    QVERIFY(result.status == shanty::SerializeResult::Status::Success);
    const QStringList lines = logicalLines(result.data);
    QVERIFY(lines.first() == "#EXTM3U");
    QVERIFY(lines.contains("/a.mp3"));
    QVERIFY(lines.contains("/b.flac"));
}

void TestM3uRoundtrip::serializesExtInfWhenKnown() {
    M3uPlaylistCodec codec;
    const shanty::SerializeResult result =
        codec.serialize(simplePlaylist(), "/x/playlist.m3u8");
    const QStringList lines = logicalLines(result.data);
    QVERIFY(lines.contains("#EXTINF:123,B Song"));
    // Plain entry without metadata must NOT carry an EXTINF with unknown duration
    const int index = lines.indexOf("/a.mp3");
    QVERIFY(index > 0);
    QVERIFY(!lines[index - 1].startsWith("#EXTINF"));
}

void TestM3uRoundtrip::roundTripKeepsEntriesAndMetadata() {
    M3uPlaylistCodec codec;
    const shanty::SerializeResult serialized =
        codec.serialize(simplePlaylist(), "/x/playlist.m3u8");
    QVERIFY(serialized.status == shanty::SerializeResult::Status::Success);

    const Playlist reparsed = parseOf(serialized.data, "/x/playlist.m3u8");
    QCOMPARE(reparsed.count(), simplePlaylist().count());
    for (int i = 0; i < int(simplePlaylist().count()); ++i) {
        QCOMPARE(QString::fromStdString(reparsed.entries()[i].path),
                 QString::fromStdString(simplePlaylist().entries()[i].path));
        QCOMPARE(QString::fromStdString(reparsed.entries()[i].title),
                 QString::fromStdString(simplePlaylist().entries()[i].title));
        QCOMPARE(reparsed.entries()[i].durationSeconds,
                 simplePlaylist().entries()[i].durationSeconds);
    }
}

void TestM3uRoundtrip::roundTripSurvivesWhitespaceDifferences() {
    // A parse→serialize→parse chain is idempotent (attribute equality) even
    // when intermediate byte layout differs (CRLF vs LF, trailing newline).
    M3uPlaylistCodec codec;
    const Playlist playlist = simplePlaylist();
    const shanty::SerializeResult first = codec.serialize(playlist, "/x/p.m3u8");
    const shanty::SerializeResult second =
        codec.serialize(parseOf(first.data, "/x/p.m3u8"), "/x/p.m3u8");
    QVERIFY(first.status == shanty::SerializeResult::Status::Success);
    QVERIFY(second.status == shanty::SerializeResult::Status::Success);
    QCOMPARE(logicalLines(first.data), logicalLines(second.data));
}

void TestM3uRoundtrip::exportTargetExtensionChoosesEncodingRule() {
    // .m3u8 target: entries must be serializable (UTF-8); .m3u target with
    // Latin-1-safe metadata succeeds as well. Both targets must round-trip
    // the same playlist through .m3u8 parsing afterwards.
    M3uPlaylistCodec codec;
    for (const char* target : {"/x/one.m3u8", "/x/two.m3u"}) {
        const shanty::SerializeResult result =
            codec.serialize(simplePlaylist(), target);
        QVERIFY(result.status == shanty::SerializeResult::Status::Success);
        const Playlist reparsed = parseOf(result.data, "/x/reparsed.m3u8");
        QCOMPARE(reparsed.count(), simplePlaylist().count());
    }
}

void TestM3uRoundtrip::latin1ExportRepresentsTwoByteCodePointsExactly() {
    // Stored metadata is UTF-8 ("Café" = 43 61 66 C3 A9). A .m3u target
    // re-encodes to real Latin-1 whenever every code point fits (é = U+00E9
    // → byte E9). The dropped-lead-bits decode produced ')' (0x29) instead.
    M3uPlaylistCodec codec;
    Playlist playlist;
    playlist.add(entry("/a.mp3", "Caf\xC3\xA9", 4));

    const shanty::SerializeResult result =
        codec.serialize(playlist, "/x/cafe.m3u");
    QVERIFY(result.status == shanty::SerializeResult::Status::Success);
    QVERIFY2(result.data.find("Caf\xE9\n") != std::string::npos,
             ".m3u export must carry the real Latin-1 byte E9 for é");
}

void TestM3uRoundtrip::latin1RoundTripIsByteLevelIdempotent() {
    M3uPlaylistCodec codec;

    // (1) A legacy .m3u payload with real Latin-1 metadata (E9) parses to
    // stored UTF-8 and re-exports to the SAME byte sequence.
    const std::string latin1Payload =
        "#EXTM3U\n#EXTINF:4,Caf\xE9\n/a.mp3\n";
    const Playlist stored = parseOf(latin1Payload, "/x/legacy.m3u");
    QCOMPARE(stored.count(), size_t(1));
    QCOMPARE(QString::fromStdString(stored.entries()[0].title),
             QString::fromUtf8("Caf\xC3\xA9"));
    const shanty::SerializeResult out1 =
        codec.serialize(stored, "/x/legacy.m3u");
    QVERIFY(out1.status == shanty::SerializeResult::Status::Success);
    QCOMPARE(out1.data, latin1Payload);

    // (2) A .m3u payload carrying UTF-8 bytes (no BOM — e.g. an earlier
    // export of a title that exceeds Latin-1) is stored under the Latin-1
    // rules (every byte becomes its own code point, re-encoded to UTF-8).
    // The export must reproduce the stored bytes verbatim (decode → Latin-1
    // refit) — the wave-8b device bug turned C3 83 into 03 here instead.
    const std::string utf8BytesPayload =
        "#EXTM3U\n#EXTINF:4,Caf\xC3\xA9\n/a.mp3\n";
    const Playlist stored2 = parseOf(utf8BytesPayload, "/x/utf8bytes.m3u");
    const shanty::SerializeResult out2 =
        codec.serialize(stored2, "/x/utf8bytes.m3u");
    QVERIFY(out2.status == shanty::SerializeResult::Status::Success);
    QCOMPARE(out2.data, utf8BytesPayload);
}

QTEST_MAIN(TestM3uRoundtrip)
#include "tst_m3u_roundtrip.moc"
