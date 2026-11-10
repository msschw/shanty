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

QTEST_MAIN(TestLibraryScan)
#include "tst_library_scan.moc"
