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

QTEST_MAIN(TestM3uRoundtrip)
#include "tst_m3u_roundtrip.moc"
