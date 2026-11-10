// SH-302 Phase A: M3U encoding rules (.m3u = Latin-1 fallback, .m3u8 = UTF-8).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QDebug>

#include "adapters/m3u/M3uPlaylistCodec.h"
#include "domain/M3uParseResult.h"

using shanty::M3uPlaylistCodec;
using shanty::ParseResult;
using shanty::PlaylistEntry;

namespace {

/// "#EXTINF:1, Möde" encoded in Latin-1 (escape split to prevent GCC
/// maximal-munch from folding \xF6 and the following hex digits).
const char kExtinfTitleLatin1[] = "#EXTM3U\n#EXTINF:1,M" "\xF6" "de\n/song.mp3\n";

QString firstTitle(const ParseResult& result) {
    if (result.playlist.count() == 0) return QString();
    return QString::fromStdString(result.playlist.entries()[0].title);
}

}  // namespace

class TestM3uEncoding : public QObject {
    Q_OBJECT

private slots:
    void m3u8DecodesAsUtf8();
    void m3uExtInfFallsBackToLatin1();
    void utf8BomInM3uIsSniffed();
    void reservedC1BytesInM3uAreDecodable();
};

void TestM3uEncoding::m3u8DecodesAsUtf8() {
    // "#EXTINF:1, Löse" in UTF-8: L\xC3\xB6se
    const std::string data =
        std::string("#EXTM3U\n#EXTINF:1,L") + "\xC3\xB6" + "se\n/song.mp3\n";
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse(data, "/x/playlist.m3u8");
    QVERIFY(result.ok());
    QCOMPARE(firstTitle(result), QString::fromUtf8("Löse"));
}

void TestM3uEncoding::m3uExtInfFallsBackToLatin1() {
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse(kExtinfTitleLatin1, "/x/playlist.m3u");
    QVERIFY(result.ok());
    QCOMPARE(firstTitle(result), QString::fromLatin1("M" "\xF6" "de"));
}

void TestM3uEncoding::utf8BomInM3uIsSniffed() {
    // A .m3u whose payload actually starts with an UTF-8 BOM must decode
    // as UTF-8, not fall back to Latin-1.
    std::string data = "\xEF\xBB\xBF";
    data += "#EXTM3U\n#EXTINF:1,\xC3\xB6se\n/song.mp3\n";
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse(data, "/x/playlist.m3u");
    QVERIFY(result.ok());
    QCOMPARE(firstTitle(result), QString::fromUtf8("öse"));
}

void TestM3uEncoding::reservedC1BytesInM3uAreDecodable() {
    // The extension matrix: .m3u payload without BOM containing a high
    // byte in metadata decodes as Latin-1 (0xFC → 'ü').
    const std::string data = "#EXTM3U\n#EXTINF:1,T" "\xFC" "r\n/song.mp3\n";
    M3uPlaylistCodec codec;
    const ParseResult result = codec.parse(data, "/x/playlist.m3u");
    QVERIFY(result.ok());
    QCOMPARE(firstTitle(result), QString::fromLatin1("T" "\xFC" "r"));
}

QTEST_MAIN(TestM3uEncoding)
#include "tst_m3u_encoding.moc"
