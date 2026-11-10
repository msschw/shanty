// SH-301 Phase A: M3U basic parsing rules.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "adapters/m3u/M3uPlaylistCodec.h"
#include "domain/M3uParseResult.h"

using shanty::M3uPlaylistCodec;
using shanty::ParseError;
using shanty::ParseResult;
using shanty::PlaylistEntry;

namespace {

QStringList paths(const ParseResult& result) {
    QStringList list;
    if (result.errors.empty()) {
        for (const PlaylistEntry& e : result.playlist.entries())
            list << QString::fromStdString(e.path);
    }
    return list;
}

}  // namespace

class TestM3uCodec : public QObject {
    Q_OBJECT

private slots:
    void parsesPlainPathLines();
    void toleratesMissingExtM3uHeader();
    void skipsCommentLines();
    void skipsBlankLines();
    void honorsExtInfTitleAndDuration();
    void normalizesCrlfLineEndings();
    void reportsErrorOnGarbageOnly();
    void emptyInputIsEmptyPlaylist();
};

void TestM3uCodec::parsesPlainPathLines() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("#EXTM3U\n/song1.mp3\n/song2.flac\n", "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(paths(result), (QStringList{"/song1.mp3", "/song2.flac"}));
}

void TestM3uCodec::toleratesMissingExtM3uHeader() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("/only/song.ogg\n", "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(paths(result), QStringList{"/only/song.ogg"});
}

void TestM3uCodec::skipsCommentLines() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse(
        "#EXTM3U\n#a comment\n/song.mp3\n#another comment\n/song2.mp3\n",
        "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(paths(result), (QStringList{"/song.mp3", "/song2.mp3"}));
}

void TestM3uCodec::skipsBlankLines() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("#EXTM3U\n\n/song.mp3\n\n\n/song2.mp3\n\n",
                                           "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(paths(result).size(), 2);
}

void TestM3uCodec::honorsExtInfTitleAndDuration() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("#EXTM3U\n#EXTINF:214,My Song\n/song.mp3\n",
                                           "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(result.playlist.count(), size_t(1));
    const PlaylistEntry e = result.playlist.entries()[0];
    QCOMPARE(QString::fromStdString(e.title), QString::fromLatin1("My Song"));
    QCOMPARE(e.durationSeconds, 214);
}

void TestM3uCodec::normalizesCrlfLineEndings() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("#EXTM3U\r\n/song1.mp3\r\n/song2.mp3\r\n",
                                           "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(paths(result), (QStringList{"/song1.mp3", "/song2.mp3"}));
    QCOMPARE(int(paths(result)[0].size()), int(QString("/song1.mp3").size()));
}

void TestM3uCodec::reportsErrorOnGarbageOnly() {
    // A document that contains no playlist content at all is an error,
    // not a silently empty playlist.
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("just some words", "/x/playlist.m3u8");
    QVERIFY(!result.errors.empty());
}

void TestM3uCodec::emptyInputIsEmptyPlaylist() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse("", "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(result.playlist.count(), size_t(0));
}

QTEST_MAIN(TestM3uCodec)
#include "tst_m3u_parse.moc"
