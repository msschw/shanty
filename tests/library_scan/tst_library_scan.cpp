// SH-202 Phase A: LibraryService scan/filtering with a fake scanner.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "domain/LibraryConfig.h"
#include "services/LibraryService.h"
#include "fakes/FakeScanner.h"

using shanty::LibraryConfig;
using shanty::LibraryService;

namespace {
QStringList toQStringList(const std::vector<std::string>& paths) {
    QStringList list;
    for (const std::string& p : paths) list << QString::fromStdString(p);
    return list;
}
}  // namespace

class TestLibraryScan : public QObject {
    Q_OBJECT

private slots:
    void scannerWithoutConfigReturnsNothing();
    void filtersUnsupportedExtensions();
    void blacklistExcludesSubfolderAndFile();
    void emptyWhitelistMeansPassThrough();
    void nonEmptyWhitelistRestrictsResults();
    void whitelistOverrulesNothingButBlacklistWinsAlways();
    void delegatesScanToScanner();
    // SH-1002 (SH-E10 UX-13): playlist discovery composition
    void playlistsDiscoveredByExtension();
    void playlistsFilteredByBlacklistAndWhitelist();
    void playlistsRequireManagedFolder();
    void playlistsDelegateToSeparateScannerQuery();
    void audioScanUnaffectedByPlaylistCandidates();

private:
    LibraryConfig baseConfig();
};

void TestLibraryScan::scannerWithoutConfigReturnsNothing() {
    FakeScanner scanner({"/music/a.mp3"});
    LibraryConfig config;  // no folders
    QCOMPARE(toQStringList(LibraryService(scanner).scan(config)), QStringList());
}

void TestLibraryScan::filtersUnsupportedExtensions() {
    FakeScanner scanner({"/music/a.mp3", "/music/note.txt", "/music/v3.ogg",
                         "/music/big.flac", "/music/movie.mp4"});
    const LibraryConfig config = baseConfig();
    const QStringList result = toQStringList(LibraryService(scanner).scan(config));
    QCOMPARE(result, (QStringList{"/music/a.mp3", "/music/v3.ogg", "/music/big.flac"}));
}

void TestLibraryScan::blacklistExcludesSubfolderAndFile() {
    FakeScanner scanner({"/music/a.mp3", "/music/skip/x.mp3", "/music/hit.mp3",
                         "/music/skip/deep/y.mp3"});
    LibraryConfig config = baseConfig();
    config.blacklist = {"/music/skip", "/music/hit.mp3"};
    const QStringList result = toQStringList(LibraryService(scanner).scan(config));
    QCOMPARE(result, QStringList{"/music/a.mp3"});
}

void TestLibraryScan::emptyWhitelistMeansPassThrough() {
    FakeScanner scanner({"/music/a.mp3", "/music/b.flac"});
    const LibraryConfig config = baseConfig();
    QCOMPARE(toQStringList(LibraryService(scanner).scan(config)).size(), 2);
}

void TestLibraryScan::nonEmptyWhitelistRestrictsResults() {
    FakeScanner scanner({"/music/a.mp3", "/music/b.mp3", "/music/c.mp3"});
    LibraryConfig config = baseConfig();
    config.whitelist = {"/music/a", "/music/c"};
    const QStringList result = toQStringList(LibraryService(scanner).scan(config));
    QCOMPARE(result, (QStringList{"/music/a.mp3", "/music/c.mp3"}));
}

void TestLibraryScan::whitelistOverrulesNothingButBlacklistWinsAlways() {
    // Documented composition: blacklist beats whitelist.
    FakeScanner scanner({"/music/a.mp3"});
    LibraryConfig config = baseConfig();
    config.whitelist = {"/music/a"};
    config.blacklist = {"/music/a.mp3"};
    QCOMPARE(toQStringList(LibraryService(scanner).scan(config)), QStringList());
}

void TestLibraryScan::delegatesScanToScanner() {
    FakeScanner scanner({});
    const LibraryConfig config = baseConfig();
    LibraryService(scanner).scan(config);
    QCOMPARE(scanner.callCount_, 1);
}

LibraryConfig TestLibraryScan::baseConfig() {
    LibraryConfig config;
    config.folders = {"/music"};
    return config;
}

// ---- SH-1002: playlist discovery (owner decision: a playlist = an
// .m3u/.m3u8 file found within the library; same rule stack as audio) ----

void TestLibraryScan::playlistsDiscoveredByExtension() {
    FakeScanner scanner({});
    scanner.playlistResult_ = {
        "/music/sub/deep.m3u", "/music/top.m3u", "/music/top.m3u8",
        "/music/song.mp3", "/music/notes.txt", "/music/set.m3u.bak"};
    const LibraryConfig config = baseConfig();
    const QStringList result =
        toQStringList(LibraryService(scanner).scanPlaylists(config));
    // Only playlist-extension candidates pass; deterministic order.
    QCOMPARE(result, (QStringList{"/music/sub/deep.m3u", "/music/top.m3u",
                                  "/music/top.m3u8"}));
}

void TestLibraryScan::playlistsFilteredByBlacklistAndWhitelist() {
    // Blacklist beats whitelist (same composition rule as audio).
    FakeScanner scanner({});
    scanner.playlistResult_ = {"/music/keep.m3u", "/music/skip/drop.m3u"};
    LibraryConfig config = baseConfig();
    config.whitelist = {"/music/keep"};
    config.blacklist = {"/music/skip"};
    const QStringList result =
        toQStringList(LibraryService(scanner).scanPlaylists(config));
    QCOMPARE(result, QStringList{"/music/keep.m3u"});

    // And the blacklist wins when both name the same path.
    FakeScanner conflict({});
    conflict.playlistResult_ = {"/music/a.m3u"};
    config = baseConfig();
    config.whitelist = {"/music"};
    config.blacklist = {"/music/a.m3u"};
    QVERIFY(LibraryService(conflict).scanPlaylists(config).empty());
}

void TestLibraryScan::playlistsRequireManagedFolder() {
    // A playlist-extension candidate outside the managed folders is
    // ignored, exactly like audio candidates (SH-202 composition).
    FakeScanner scanner({});
    scanner.playlistResult_ = {"/elsewhere/playlist.m3u",
                               "/music/inside.m3u"};
    LibraryConfig config = baseConfig();
    config.folders = {"/music"};
    const QStringList result =
        toQStringList(LibraryService(scanner).scanPlaylists(config));
    QCOMPARE(result, QStringList{"/music/inside.m3u"});
}

void TestLibraryScan::playlistsDelegateToSeparateScannerQuery() {
    // Interface Segregation: scanPlaylists queries the scanner's
    // playlist enumeration (exactly once), NOT scan() — the audio
    // contract stays untouched.
    FakeScanner scanner({});
    const LibraryConfig config = baseConfig();
    LibraryService(scanner).scanPlaylists(config);
    QCOMPARE(scanner.playlistCallCount_, 1);
    QCOMPARE(scanner.callCount_, 0);
}

void TestLibraryScan::audioScanUnaffectedByPlaylistCandidates() {
    // Regression pin: .m3u candidates in the audio enumeration are NOT
    // audio files and never appear in scan() output.
    FakeScanner scanner({"/music/song.mp3", "/music/set.m3u",
                         "/music/set.m3u8"});
    const LibraryConfig config = baseConfig();
    const QStringList result =
        toQStringList(LibraryService(scanner).scan(config));
    QCOMPARE(result, QStringList{"/music/song.mp3"});
}

QTEST_MAIN(TestLibraryScan)
#include "tst_library_scan.moc"
