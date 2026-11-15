// SH-1003 Phase A: LibraryCatalogueViewModel — the media-browser grouping
// layer (SH-E10 UX-14). Red-first: the view model is a stub, every case
// fails by assertion (never a crash).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "FakePersistingLibraryStore.h"
#include "FakeTagReader.h"
#include "RecordingScanner.h"

#include "core/services/LibraryService.h"
#include "presentation/viewmodels/LibraryCatalogue.h"

using shanty::CatalogueGroup;
using shanty::CatalogueMode;
using shanty::LibraryCatalogueViewModel;
using shanty::LibraryConfig;
using shanty::TagMetadata;

namespace {

/// Shared fixture: recording scanner + persisting store + scripted reader.
/// The store is seeded BEFORE the view model is constructed (the ctor
/// loads — the settings_vm Phase A lesson). Callers script the scan
/// results and the tags, then construct through build().
struct Vm {
    RecordingScanner scanner;
    shanty::LibraryService library;
    FakePersistingLibraryStore store;
    FakeTagReader reader;

    explicit Vm(std::vector<std::string> tracks = {},
                std::vector<std::string> playlists = {},
                std::map<std::string, TagMetadata> tags = {})
        : scanner(std::move(tracks)),
          library(scanner) {
        scanner.playlistResult_ = std::move(playlists);
        reader.scripted_ = std::move(tags);
    }

    LibraryCatalogueViewModel make() {
        LibraryConfig config;
        config.folders = {"/music"};
        store.save(config);
        return LibraryCatalogueViewModel(store, library, reader);
    }

    void rebuild(LibraryCatalogueViewModel& vm) { vm.rebuild(); }
};

TagMetadata tags(const std::string& title, const std::string& artist,
                 const std::string& album, const std::string& genre) {
    TagMetadata meta;
    meta.title = title;
    meta.artist = artist;
    meta.album = album;
    meta.genre = genre;
    return meta;
}

}  // namespace

class TestLibraryCatalogue : public QObject {
    Q_OBJECT

private slots:
    void emptyLibraryYieldsAllModesEmpty();
    void artistsModeIsTwoLevels();
    void missingTagsLandInVerbatimUnknownBuckets();
    void categoriesAreAlphabeticalCaseInsensitive();
    void tracksOrderByDisplayTitleCaseInsensitiveWithBasenameFallback();
    void albumsModeIsFlatOneGroupPerAlbum();
    void genresModeIsFlatOneGroupPerGenre();
    void playlistsAreFlatInScanOrder();
    void rebuildRereadsConfigAndRescansThroughTheStore();
    void tagsReadExactlyOncePerTrackPerRebuild();
};

void TestLibraryCatalogue::emptyLibraryYieldsAllModesEmpty() {
    Vm fix;
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    QVERIFY(vm.groups(CatalogueMode::Artists).empty());
    QVERIFY(vm.groups(CatalogueMode::Albums).empty());
    QVERIFY(vm.groups(CatalogueMode::Genres).empty());
    QVERIFY(vm.playlistPaths().empty());
}

void TestLibraryCatalogue::artistsModeIsTwoLevels() {
    // Owner decision 2026-11-14: Artists browses artist → their albums →
    // tracks. Same album under one artist groups once; the same album name
    // under a DIFFERENT artist stays separate (the child belongs to its
    // artist, not to the album name).
    Vm fix(
        {"/music/t1.mp3", "/music/t2.mp3", "/music/t3.mp3", "/music/t4.mp3"},
        {},
        {{"/music/t1.mp3", tags("Alpha", "Aurora", "Sky", "Pop")},
         {"/music/t2.mp3", tags("Beta", "Aurora", "Sky", "Pop")},
         {"/music/t3.mp3", tags("Gamma", "Aurora", "Land", "Pop")},
         {"/music/t4.mp3", tags("Delta", "Beacon", "Sky", "Rock")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    const std::vector<CatalogueGroup>& artists =
        vm.groups(CatalogueMode::Artists);
    QCOMPARE(int(artists.size()), 2);
    // Artist ordering case-insensitive alphabetical (Aurora < Beacon).
    QVERIFY(artists[0].label == "Aurora");
    QVERIFY(artists[1].label == "Beacon");
    // Artists own NO leaf paths — everything lives in the album children.
    QVERIFY(artists[0].paths.empty());
    // Aurora's albums: Land before Sky (case-insensitive alphabetical).
    QCOMPARE(int(artists[0].children.size()), 2);
    QVERIFY(artists[0].children[0].label == "Land");
    QVERIFY(artists[0].children[1].label == "Sky");
    // Album children hold the track paths, ordered by display title.
    QVERIFY(artists[0].children[0].paths ==
            std::vector<std::string>({"/music/t3.mp3"}));
    QVERIFY(artists[0].children[1].paths ==
            std::vector<std::string>({"/music/t1.mp3", "/music/t2.mp3"}));
    // Beacon's "Sky" is a separate album group (two-level structure).
    QCOMPARE(int(artists[1].children.size()), 1);
    QVERIFY(artists[1].children[0].label == "Sky");
    QVERIFY(artists[1].children[0].paths ==
            std::vector<std::string>({"/music/t4.mp3"}));
}

void TestLibraryCatalogue::missingTagsLandInVerbatimUnknownBuckets() {
    // Decision 3: the Unknown strings are decided once, verbatim; a clean
    // miss lands in all three Unknown buckets, a partial tag only in the
    // missing ones. Unknown buckets participate in the normal ordering.
    Vm fix({"/music/untagged.mp3", "/music/artonly.mp3"},
           {},
           {{"/music/artonly.mp3", tags("Solo", "Lantern", "", "")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    // The catalogue strings themselves (stable for the QML layer).
    QVERIFY(std::string(LibraryCatalogueViewModel::unknownArtistLabel()) ==
            "Unknown artist");
    QVERIFY(std::string(LibraryCatalogueViewModel::unknownAlbumLabel()) ==
            "Unknown album");
    QVERIFY(std::string(LibraryCatalogueViewModel::unknownGenreLabel()) ==
            "Unknown genre");

    const std::vector<CatalogueGroup>& artists =
        vm.groups(CatalogueMode::Artists);
    QCOMPARE(int(artists.size()), 2);
    // "Lantern" < "Unknown artist" case-insensitively — no special-casing.
    QVERIFY(artists[0].label == "Lantern");
    QVERIFY(artists[1].label ==
            LibraryCatalogueViewModel::unknownArtistLabel());
    QVERIFY(artists[1].children.size() == 1);
    QVERIFY(artists[1].children[0].label ==
            LibraryCatalogueViewModel::unknownAlbumLabel());
    QVERIFY(artists[1].children[0].paths ==
            std::vector<std::string>({"/music/untagged.mp3"}));

    // Albums mode: the untagged file sits in "Unknown album".
    const std::vector<CatalogueGroup>& albums =
        vm.groups(CatalogueMode::Albums);
    QCOMPARE(int(albums.size()), 2);
    QVERIFY(albums[0].label == LibraryCatalogueViewModel::unknownAlbumLabel());
    QVERIFY(albums[0].paths == std::vector<std::string>({"/music/untagged.mp3"}));

    // Genres mode: both files lack a genre → one "Unknown genre" group.
    const std::vector<CatalogueGroup>& genres =
        vm.groups(CatalogueMode::Genres);
    QCOMPARE(int(genres.size()), 1);
    QVERIFY(genres[0].label == LibraryCatalogueViewModel::unknownGenreLabel());
    QCOMPARE(int(genres[0].paths.size()), 2);
}

void TestLibraryCatalogue::categoriesAreAlphabeticalCaseInsensitive() {
    // Decision 5: byte-order comparison would put "apple" after "Zebra"
    // (uppercase < lowercase); case-insensitively "apple" < "banana" <
    // "cherry" < "Zebra" holds. Same rule for albums and genres labels.
    Vm fix({"/music/t1.mp3", "/music/t2.mp3", "/music/t3.mp3",
            "/music/t4.mp3", "/music/t5.mp3"},
           {},
           {{"/music/t1.mp3", tags("a", "cherry", "", "")},
            {"/music/t2.mp3", tags("b", "apple", "", "")},
            {"/music/t3.mp3", tags("c", "Zebra", "", "")},
            {"/music/t4.mp3", tags("d", "banana", "", "")},
            {"/music/t5.mp3", tags("e", "apple", "", "")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    const std::vector<CatalogueGroup>& artists =
        vm.groups(CatalogueMode::Artists);
    QCOMPARE(int(artists.size()), 4);
    QVERIFY(artists[0].label == "apple");
    QVERIFY(artists[1].label == "banana");
    QVERIFY(artists[2].label == "cherry");
    QVERIFY(artists[3].label == "Zebra");
}

void TestLibraryCatalogue::
    tracksOrderByDisplayTitleCaseInsensitiveWithBasenameFallback() {
    // Decision 4/5: display title = tag title, falling back to the basename
    // without extension (Track::make rule); ordering case-insensitive.
    Vm fix({"/music/mix/fall.mp3", "/music/mix/zeta.mp3",
            "/music/mix/Zebra.mp3"},
           {},
           {{"/music/mix/zeta.mp3", tags("zeta", "One", "Mixed", "")},
            {"/music/mix/Zebra.mp3", tags("Zebra", "One", "Mixed", "")}});
    // /music/mix/fall.mp3 is untagged → display title "fall" (basename).
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    const std::vector<CatalogueGroup>& albums =
        vm.groups(CatalogueMode::Albums);
    QCOMPARE(int(albums.size()), 1);
    QVERIFY(albums[0].label == "Mixed");
    // fall < zebra < zeta (case-insensitive).
    QVERIFY(albums[0].paths ==
            std::vector<std::string>({"/music/mix/fall.mp3",
                                      "/music/mix/Zebra.mp3",
                                      "/music/mix/zeta.mp3"}));
}

void TestLibraryCatalogue::albumsModeIsFlatOneGroupPerAlbum() {
    // Decision 2: Albums is ONE level — label → track paths, no children.
    Vm fix({"/music/t1.mp3", "/music/t2.mp3"},
           {},
           {{"/music/t1.mp3", tags("Alpha", "Aurora", "Sky", "")},
            {"/music/t2.mp3", tags("Beta", "Beacon", "Land", "")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    const std::vector<CatalogueGroup>& albums =
        vm.groups(CatalogueMode::Albums);
    QCOMPARE(int(albums.size()), 2);
    QVERIFY(albums[0].label == "Land");
    QVERIFY(albums[0].paths == std::vector<std::string>({"/music/t2.mp3"}));
    QVERIFY(albums[1].label == "Sky");
    QVERIFY(albums[1].paths == std::vector<std::string>({"/music/t1.mp3"}));
    QVERIFY(albums[0].children.empty());
    QVERIFY(albums[1].children.empty());
}

void TestLibraryCatalogue::genresModeIsFlatOneGroupPerGenre() {
    // Decision 2: Genres is ONE level, same shape as Albums.
    Vm fix({"/music/t1.mp3", "/music/t2.mp3", "/music/t3.mp3"},
           {},
           {{"/music/t1.mp3", tags("Alpha", "A", "B", "Rock")},
            {"/music/t2.mp3", tags("Beta", "A", "B", "Jazz")},
            {"/music/t3.mp3", tags("Gamma", "C", "D", "Rock")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    const std::vector<CatalogueGroup>& genres =
        vm.groups(CatalogueMode::Genres);
    QCOMPARE(int(genres.size()), 2);
    QVERIFY(genres[0].label == "Jazz");
    QVERIFY(genres[0].paths == std::vector<std::string>({"/music/t2.mp3"}));
    QVERIFY(genres[1].label == "Rock");
    QVERIFY(genres[1].paths ==
            std::vector<std::string>({"/music/t1.mp3", "/music/t3.mp3"}));
    QVERIFY(genres[0].children.empty());
}

void TestLibraryCatalogue::playlistsAreFlatInScanOrder() {
    // Decision 2: Playlists mode is NOT grouped — SH-1002's discovery
    // results as a flat path list, the scanner's order verbatim.
    Vm fix({}, {"/music/lists/b.m3u", "/music/lists/a.m3u8"});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();

    QVERIFY(vm.playlistPaths() ==
            std::vector<std::string>({"/music/lists/b.m3u",
                                      "/music/lists/a.m3u8"}));
    // And no grouping mode surfaces from a playlist-only library.
    QVERIFY(vm.groups(CatalogueMode::Artists).empty());
    QVERIFY(vm.groups(CatalogueMode::Albums).empty());
    QVERIFY(vm.groups(CatalogueMode::Genres).empty());
}

void TestLibraryCatalogue::rebuildRereadsConfigAndRescansThroughTheStore() {
    // Decision 1/6: rebuild() loads the config through the store AGAIN (the
    // cross-visibility read path with SH-1004) and rescans — exactly one
    // store.load(), one scan() and one scanPlaylists() per rebuild, with
    // the CURRENT persisted folders.
    Vm fix({"/music/t1.mp3"}, {"/music/lists/x.m3u"});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();
    QCOMPARE(fix.store.loadCount_, 1);
    QCOMPARE(fix.scanner.callCount_, 1);
    QCOMPARE(fix.scanner.playlistCallCount_, 1);
    QCOMPARE(int(fix.scanner.scannedFolders_.back().size()), 1);
    QVERIFY(fix.scanner.scannedFolders_.back()[0] == "/music");

    // Settings side changed the shared config (SH-1004).
    LibraryConfig updated;
    updated.folders = {"/music", "/podcasts"};
    fix.store.save(updated);
    vm.rebuild();

    QCOMPARE(fix.store.loadCount_, 2);
    QCOMPARE(fix.scanner.callCount_, 2);
    QCOMPARE(fix.scanner.playlistCallCount_, 2);
    QCOMPARE(int(fix.scanner.scannedFolders_.back().size()), 2);
    QVERIFY(fix.scanner.scannedFolders_.back()[1] == "/podcasts");
}

void TestLibraryCatalogue::tagsReadExactlyOncePerTrackPerRebuild() {
    // Decision 1: one tag read per track per rebuild — no caching between
    // rebuilds, and no double reads within one rebuild.
    Vm fix({"/music/t1.mp3", "/music/t2.mp3"},
           {},
           {{"/music/t1.mp3", tags("Alpha", "Aurora", "Sky", "")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();
    QCOMPARE(fix.reader.readsFor("/music/t1.mp3"), 1);
    QCOMPARE(fix.reader.readsFor("/music/t2.mp3"), 1);

    vm.rebuild();
    // Re-read at scan time: exactly one MORE read per track per rebuild.
    QCOMPARE(fix.reader.readsFor("/music/t1.mp3"), 2);
    QCOMPARE(fix.reader.readsFor("/music/t2.mp3"), 2);
    QCOMPARE(fix.reader.callCount_, 4);
}

QTEST_MAIN(TestLibraryCatalogue)
#include "tst_library_catalogue.moc"
