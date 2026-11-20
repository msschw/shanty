// SH-1003 Phase A: LibraryCatalogueViewModel — the media-browser grouping
// layer (SH-E10 UX-14). Red-first: the view model is a stub, every case
// fails by assertion (never a crash).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <memory>

#include "FakePersistingLibraryStore.h"
#include "FakeTagReader.h"
#include "FakeArtworkReader.h"
#include "RecordingScanner.h"

#include "core/services/LibraryService.h"
#include "presentation/viewmodels/LibraryBrowser.h"
#include "presentation/viewmodels/LibraryCatalogue.h"

using shanty::BrowserMode;
using shanty::CatalogueGroup;
using shanty::CatalogueMode;
using shanty::LibraryBrowserViewModel;
using shanty::LibraryCatalogueViewModel;
using shanty::LibraryConfig;
using shanty::TagMetadata;

namespace {

QString asQ(const std::string& s) { return QString::fromStdString(s); }

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

// ---- SH-1005 browser fixtures ------------------------------------------------

/// Browser over a living catalogue: catalogue on the heap (the browser
/// holds a reference), rejection log, one-line rebuild passthrough.
struct BrowserFix {
    Vm fix;
    std::unique_ptr<LibraryCatalogueViewModel> catalogue;
    std::unique_ptr<LibraryBrowserViewModel> browser;
    FakeArtworkReader artwork;
    std::vector<std::string> invalid;

    explicit BrowserFix(std::vector<std::string> tracks = {},
                        std::map<std::string, TagMetadata> tagMap = {},
                        std::vector<std::string> playlists = {})
        : fix(std::move(tracks), std::move(playlists), std::move(tagMap)) {
        catalogue.reset(new LibraryCatalogueViewModel(fix.make()));
        browser.reset(new LibraryBrowserViewModel(*catalogue, &artwork));
        browser->onInvalid = [this](const char* reason) {
            invalid.push_back(reason ? reason : "(null)");
        };
    }

    void rebuild() {
        catalogue->rebuild();
        browser->rebuild();
    }
};

/// The standard artists fixture: Aurora (Land: Gamma; Sky: Alpha, Beta),
/// Beacon (Zoo: Delta), one untagged file (all Unknown buckets, title
/// fallback "t5"). /music/t5.mp3 has NO scripted tags (clean miss).
BrowserFix artistsFixture() {
    std::vector<std::string> tracks = {"/music/t1.mp3", "/music/t2.mp3",
                                       "/music/t3.mp3", "/music/t4.mp3",
                                       "/music/t5.mp3"};
    std::map<std::string, TagMetadata> tagMap = {
        {"/music/t1.mp3", tags("Alpha", "Aurora", "Sky", "Pop")},
        {"/music/t2.mp3", tags("Beta", "Aurora", "Sky", "Pop")},
        {"/music/t3.mp3", tags("Gamma", "Aurora", "Land", "Pop")},
        {"/music/t4.mp3", tags("Delta", "Beacon", "Zoo", "Rock")},
    };
    return BrowserFix(tracks, tagMap);
}

int indexOfRow(const BrowserFix& fix, const std::string& label) {
    for (std::size_t i = 0; i < fix.browser->rows().size(); ++i) {
        if (fix.browser->rows()[i].label == label) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

shanty::Artwork picture() {
    shanty::Artwork art;
    art.mimeType = "image/png";
    art.data = "png";
    return art;
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
    void displayTitlesDecidedAtRebuildWithoutExtraReads();

    // ---- SH-1005 (UX-16): browser state layer (red-first) --------------------
    void browserInitialModeIsArtistsAndSetModeResetsDrillDown();
    void browserArtistsDrillDownTwoLevelsAndBack();
    void browserAlbumsAndGenresDrillStraightToTracks();
    void browserPlaylistsModeListsPlaylistsFlat();
    void browserOpenEntryOutOfRangeRejected();
    void browserBackAtRootIsNoOp();
    void browserRebuildResetsNavigationAndRefreshesRows();
    void browserRowCommandsCatalogue();

    // ---- SH-1009 (UX-20): artwork tiles (red-first) --------------------------
    void artworkCategoryRowsUseFirstTrackWithPicture();
    void artworkTrackRowsPreferOwnPictureAndBorrowWithinOneAlbum();
    void artworkGenresLeafNeverBorrowsAcrossAlbums();
    void artworkPlaylistRowsHaveNoArtwork();
    void artworkPresenceIsCachedWithinARebuild();
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

    // Albums mode: both files lack an album tag → ONE flat "Unknown album"
    // group holding both, leaf-ordered by display title ("Solo" then the
    // untagged basename "untagged").
    const std::vector<CatalogueGroup>& albums =
        vm.groups(CatalogueMode::Albums);
    QCOMPARE(int(albums.size()), 1);
    QVERIFY(albums[0].label == LibraryCatalogueViewModel::unknownAlbumLabel());
    QVERIFY(albums[0].paths ==
            std::vector<std::string>({"/music/artonly.mp3",
                                      "/music/untagged.mp3"}));

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
    // All three files share the album tag "Mixed"; "fall" has NO title tag
    // (empty title field) → its display title comes from the basename.
    Vm fix({"/music/mix/fall.mp3", "/music/mix/zeta.mp3",
            "/music/mix/Zebra.mp3"},
           {},
           {{"/music/mix/fall.mp3", tags("", "One", "Mixed", "")},
            {"/music/mix/zeta.mp3", tags("zeta", "One", "Mixed", "")},
            {"/music/mix/Zebra.mp3", tags("Zebra", "One", "Mixed", "")}});
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

void TestLibraryCatalogue::displayTitlesDecidedAtRebuildWithoutExtraReads() {
    // SH-1005: the browser's track rows show the display titles the
    // catalogue decided at rebuild time (decision 4) — no extra tag reads.
    Vm fix({"/music/t1.mp3", "/music/t5.mp3"},
           {},
           {{"/music/t1.mp3", tags("Alpha", "Aurora", "Sky", "Pop")}});
    LibraryCatalogueViewModel vm = fix.make();
    vm.rebuild();
    QCOMPARE(fix.reader.callCount_, 2);  // one per track, unchanged
    QCOMPARE(int(vm.displayTitles().size()), 2);
    QCOMPARE(asQ(vm.displayTitles().at("/music/t1.mp3")),
             QString("Alpha"));
    // The untagged file falls back to the basename (Track::make rule).
    QCOMPARE(asQ(vm.displayTitles().at("/music/t5.mp3")), QString("t5"));
}

// ---- SH-1005 (UX-16): browser state layer (red-first) ------------------------

void TestLibraryCatalogue::browserInitialModeIsArtistsAndSetModeResetsDrillDown() {
    BrowserFix fix = artistsFixture();
    // B1: the initial mode is Artists; rows are empty until the first
    // rebuild (snapshot idiom).
    QCOMPARE(fix.browser->mode(), BrowserMode::Artists);
    QVERIFY(fix.browser->rows().empty());
    fix.rebuild();
    QCOMPARE(int(fix.browser->rows().size()), 3);
    QCOMPARE(fix.browser->rows()[0].kind,
             LibraryBrowserViewModel::Row::Kind::Category);
    QVERIFY(fix.browser->rows()[0].label == "Aurora");
    QVERIFY(fix.browser->rows()[1].label == "Beacon");
    QVERIFY(fix.browser->rows()[2].label == "Unknown artist");

    // Drill in, then re-select the mode → back to the root (B1).
    fix.browser->openEntry(0);
    QVERIFY(fix.browser->canGoBack());
    fix.browser->setMode(BrowserMode::Artists);
    QVERIFY(!fix.browser->canGoBack());
    QCOMPARE(int(fix.browser->rows().size()), 3);

    // B1: switching the mode resets the drill-down as well.
    fix.browser->openEntry(0);
    fix.browser->setMode(BrowserMode::Albums);
    QVERIFY(!fix.browser->canGoBack());
    // Flat album categories, case-insensitive order:
    // Land, Sky, Unknown album, Zoo.
    QCOMPARE(int(fix.browser->rows().size()), 4);
    QVERIFY(fix.browser->rows()[0].label == "Land");
    QVERIFY(fix.browser->rows()[1].label == "Sky");
    QVERIFY(fix.browser->rows()[2].label == "Unknown album");
    QVERIFY(fix.browser->rows()[3].label == "Zoo");
}

void TestLibraryCatalogue::browserArtistsDrillDownTwoLevelsAndBack() {
    BrowserFix fix = artistsFixture();
    fix.rebuild();

    // Level 1: artist → albums (owner decision: two levels).
    fix.browser->openEntry(0);  // Aurora
    QVERIFY(fix.browser->canGoBack());
    QCOMPARE(int(fix.browser->rows().size()), 2);
    QVERIFY(fix.browser->rows()[0].label == "Land");
    QVERIFY(fix.browser->rows()[1].label == "Sky");
    QCOMPARE(fix.browser->rows()[0].kind,
             LibraryBrowserViewModel::Row::Kind::Category);

    // Level 2: album → track rows (labels = display titles, paths ready
    // for the play bridge).
    fix.browser->openEntry(1);  // Sky
    QCOMPARE(int(fix.browser->rows().size()), 2);
    QCOMPARE(fix.browser->rows()[0].kind,
             LibraryBrowserViewModel::Row::Kind::Track);
    QVERIFY(fix.browser->rows()[0].label == "Alpha");
    QVERIFY(fix.browser->rows()[0].path == "/music/t1.mp3");
    QVERIFY(fix.browser->rows()[1].label == "Beta");
    QVERIFY(fix.browser->rows()[1].path == "/music/t2.mp3");

    // B4: a leaf level has nothing to drill into — openEntry there is a
    // silent no-op (tap semantics belong to the bridges).
    fix.browser->openEntry(0);
    QCOMPARE(int(fix.browser->rows().size()), 2);
    QVERIFY(fix.browser->rows()[0].label == "Alpha");
    QCOMPARE(int(fix.invalid.size()), 0);

    // B5: back walks the chain up one level at a time.
    fix.browser->back();
    QCOMPARE(int(fix.browser->rows().size()), 2);  // Aurora's albums
    QVERIFY(fix.browser->rows()[0].label == "Land");
    fix.browser->back();
    QVERIFY(!fix.browser->canGoBack());
    QCOMPARE(int(fix.browser->rows().size()), 3);  // artists root
}

void TestLibraryCatalogue::browserAlbumsAndGenresDrillStraightToTracks() {
    BrowserFix fix = artistsFixture();
    fix.rebuild();

    // Albums: one group level → tracks.
    fix.browser->setMode(BrowserMode::Albums);
    fix.browser->openEntry(1);  // Sky
    QCOMPARE(int(fix.browser->rows().size()), 2);
    QCOMPARE(fix.browser->rows()[0].kind,
             LibraryBrowserViewModel::Row::Kind::Track);
    QVERIFY(fix.browser->rows()[0].path == "/music/t1.mp3");
    QVERIFY(fix.browser->rows()[1].path == "/music/t2.mp3");
    fix.browser->back();
    QCOMPARE(int(fix.browser->rows().size()), 4);

    // Genres: same shape.
    fix.browser->setMode(BrowserMode::Genres);
    QCOMPARE(int(fix.browser->rows().size()), 3);
    QVERIFY(fix.browser->rows()[0].label == "Pop");
    QVERIFY(fix.browser->rows()[1].label == "Rock");
    QVERIFY(fix.browser->rows()[2].label == "Unknown genre");
    fix.browser->openEntry(0);  // Pop
    QCOMPARE(int(fix.browser->rows().size()), 3);
    QCOMPARE(fix.browser->rows()[0].kind,
             LibraryBrowserViewModel::Row::Kind::Track);
    QVERIFY(fix.browser->rows()[0].path == "/music/t1.mp3");
    QVERIFY(fix.browser->rows()[1].path == "/music/t2.mp3");
    QVERIFY(fix.browser->rows()[2].path == "/music/t3.mp3");
}

void TestLibraryCatalogue::browserPlaylistsModeListsPlaylistsFlat() {
    BrowserFix fix({}, {},
                   {"/music/lists/b.m3u", "/music/lists/a.m3u8"});
    fix.rebuild();
    fix.browser->setMode(BrowserMode::Playlists);

    // Decision 2 (SH-1003) verbatim: NOT grouped, the scanner's order.
    QCOMPARE(int(fix.browser->rows().size()), 2);
    QCOMPARE(fix.browser->rows()[0].kind,
             LibraryBrowserViewModel::Row::Kind::Playlist);
    QVERIFY(fix.browser->rows()[0].label == "/music/lists/b.m3u");
    QVERIFY(fix.browser->rows()[0].path == "/music/lists/b.m3u");
    QVERIFY(fix.browser->rows()[1].path == "/music/lists/a.m3u8");
    QVERIFY(!fix.browser->canGoBack());  // no group levels in this mode
}

void TestLibraryCatalogue::browserOpenEntryOutOfRangeRejected() {
    BrowserFix fix = artistsFixture();
    fix.rebuild();

    // B4: out-of-range is a rejection, NOT a silent no-op (house rule).
    fix.browser->openEntry(99);
    QCOMPARE(int(fix.invalid.size()), 1);
    QVERIFY(fix.invalid[0] == "openEntry: index out of range");
    QVERIFY(!fix.browser->canGoBack());
    QCOMPARE(int(fix.browser->rows().size()), 3);  // unchanged

    // An empty library: every openEntry is out of range.
    BrowserFix empty;
    empty.rebuild();
    empty.browser->openEntry(0);
    QCOMPARE(int(empty.invalid.size()), 1);
    QVERIFY(empty.invalid[0] == "openEntry: index out of range");
}

void TestLibraryCatalogue::browserBackAtRootIsNoOp() {
    BrowserFix fix = artistsFixture();
    fix.rebuild();
    QVERIFY(!fix.browser->canGoBack());
    fix.browser->back();  // no rejection, no change (B5)
    QCOMPARE(int(fix.invalid.size()), 0);
    QCOMPARE(int(fix.browser->rows().size()), 3);
    QVERIFY(fix.browser->rows()[0].label == "Aurora");
}

void TestLibraryCatalogue::browserRebuildResetsNavigationAndRefreshesRows() {
    BrowserFix fix = artistsFixture();
    fix.rebuild();
    fix.browser->openEntry(0);
    QVERIFY(fix.browser->canGoBack());

    // The library changed (Settings added a folder with new content).
    // The folder must be persisted like any settings-side add (SH-202:
    // a scanner result is not a listing until the folder is managed).
    LibraryConfig updated;
    updated.folders = {"/music", "/music2"};
    fix.fix.store.save(updated);
    fix.fix.scanner.result_ = {"/music2/new.mp3"};
    fix.fix.reader.scripted_["/music2/new.mp3"] =
        tags("Fresh", "Solo", "Debut", "Ambient");
    fix.rebuild();  // B7: passthrough + navigation reset

    QVERIFY(!fix.browser->canGoBack());
    QCOMPARE(fix.browser->mode(), BrowserMode::Artists);  // mode kept
    QCOMPARE(int(fix.browser->rows().size()), 1);
    QVERIFY(fix.browser->rows()[0].label == "Solo");
}

void TestLibraryCatalogue::browserRowCommandsCatalogue() {
    BrowserFix fix = artistsFixture();
    fix.rebuild();

    // B6: category rows have no commands (nothing to act on).
    QVERIFY(fix.browser->rowCommands(0).empty());
    QVERIFY(fix.browser->rowCommands(99).empty());  // do-not-open signal

    // Track rows: the LibraryViewModel catalogue, reused verbatim.
    fix.browser->setMode(BrowserMode::Albums);
    fix.browser->openEntry(1);  // Sky → tracks
    QVERIFY(fix.browser->rowCommands(0) ==
            std::vector<shanty::LibraryViewModel::RowCommand>(
                {shanty::LibraryViewModel::RowCommand::Play,
                 shanty::LibraryViewModel::RowCommand::AddToQueue}));
    QVERIFY(fix.browser->rowCommands(99).empty());

    // Playlist rows: play only (queue + play via the SH-802 bridge).
    BrowserFix lists({}, {}, {"/music/lists/x.m3u"});
    lists.rebuild();
    lists.browser->setMode(BrowserMode::Playlists);
    QVERIFY(lists.browser->rowCommands(0) ==
            std::vector<shanty::LibraryViewModel::RowCommand>(
                {shanty::LibraryViewModel::RowCommand::Play}));
}

// ---- SH-1009 (UX-20): artwork tiles ----------------------------------------

void TestLibraryCatalogue::artworkCategoryRowsUseFirstTrackWithPicture() {
    // Artist art = the first picture in the artist's album order; album
    // art = the first picture in the album's track order.
    BrowserFix fix = artistsFixture();
    fix.artwork.scripted_["/music/t3.mp3"] = picture();
    fix.rebuild();

    int row = indexOfRow(fix, "Aurora");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(fix.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t3.mp3"));
    row = indexOfRow(fix, "Beacon");
    QVERIFY(row >= 0);
    QVERIFY(fix.browser->rows()[row].artworkPath.empty());

    // Drilling into the artist: Land gets t3's picture, Sky has none yet.
    fix.browser->openEntry(indexOfRow(fix, "Aurora"));
    row = indexOfRow(fix, "Land");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(fix.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t3.mp3"));
    row = indexOfRow(fix, "Sky");
    QVERIFY(row >= 0);
    QVERIFY(fix.browser->rows()[row].artworkPath.empty());

    // A picture on the album's SECOND track: the album row takes it (the
    // first track has none), independently of track order. Fresh fixture:
    // the presence cache is per session, so scripting a file after a
    // cached miss is not a meaningful state.
    BrowserFix albums = artistsFixture();
    albums.artwork.scripted_["/music/t2.mp3"] = picture();
    albums.rebuild();
    albums.browser->setMode(BrowserMode::Albums);
    row = indexOfRow(albums, "Sky");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(albums.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t2.mp3"));
}

void TestLibraryCatalogue::artworkTrackRowsPreferOwnPictureAndBorrowWithinOneAlbum() {
    // Artists → Aurora → Sky is a single-album level: a track without its
    // own picture may use a sibling's (the album cover).
    BrowserFix fix = artistsFixture();
    fix.artwork.scripted_["/music/t1.mp3"] = picture();
    fix.rebuild();

    fix.browser->openEntry(indexOfRow(fix, "Aurora"));
    fix.browser->openEntry(indexOfRow(fix, "Sky"));
    int row = indexOfRow(fix, "Alpha");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(fix.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t1.mp3"));
    row = indexOfRow(fix, "Beta");  // no own picture → sibling cover
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(fix.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t1.mp3"));

    // Own picture always wins over a sibling's.
    BrowserFix own = artistsFixture();
    own.artwork.scripted_["/music/t1.mp3"] = picture();
    own.artwork.scripted_["/music/t2.mp3"] = picture();
    own.rebuild();
    own.browser->openEntry(indexOfRow(own, "Aurora"));
    own.browser->openEntry(indexOfRow(own, "Sky"));
    row = indexOfRow(own, "Beta");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(own.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t2.mp3"));
}

void TestLibraryCatalogue::artworkGenresLeafNeverBorrowsAcrossAlbums() {
    // The Genres leaf mixes albums: borrowing a sibling's picture there
    // would show the wrong album's cover.
    BrowserFix fix = artistsFixture();
    fix.artwork.scripted_["/music/t1.mp3"] = picture();
    fix.rebuild();

    fix.browser->setMode(BrowserMode::Genres);
    fix.browser->openEntry(indexOfRow(fix, "Pop"));  // t1, t2, t3
    int row = indexOfRow(fix, "Alpha");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(fix.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t1.mp3"));
    row = indexOfRow(fix, "Beta");  // no own picture; no borrow here
    QVERIFY(row >= 0);
    QVERIFY(fix.browser->rows()[row].artworkPath.empty());

    // Albums mode is the single-album contrast (Sky: t1, t2).
    fix.browser->setMode(BrowserMode::Albums);
    fix.browser->openEntry(indexOfRow(fix, "Sky"));
    row = indexOfRow(fix, "Beta");
    QVERIFY(row >= 0);
    QCOMPARE(QString::fromStdString(fix.browser->rows()[row].artworkPath),
             QStringLiteral("/music/t1.mp3"));
}

void TestLibraryCatalogue::artworkPlaylistRowsHaveNoArtwork() {
    BrowserFix fix({}, {}, {"/music/lists/x.m3u", "/music/lists/y.m3u"});
    fix.artwork.scripted_["/music/lists/x.m3u"] = picture();
    fix.rebuild();
    fix.browser->setMode(BrowserMode::Playlists);
    QCOMPARE(int(fix.browser->rows().size()), 2);
    for (const LibraryBrowserViewModel::Row& row : fix.browser->rows()) {
        QVERIFY(row.artworkPath.empty());
    }
    QCOMPARE(fix.artwork.callCount_, 0);  // playlists never consult artwork
}

void TestLibraryCatalogue::artworkPresenceIsCachedWithinARebuild() {
    // One repopulate may look at the same path from several rows (artist
    // root → album level → track level). The presence answer is cached
    // per rebuild; only a rebuild (rescan) invalidates it.
    BrowserFix fix = artistsFixture();
    fix.artwork.scripted_["/music/t2.mp3"] = picture();
    fix.rebuild();  // root: t3 miss, t1 miss, t2 hit; t4 miss

    fix.browser->openEntry(indexOfRow(fix, "Aurora"));  // Land t3, Sky t1/t2
    fix.browser->openEntry(indexOfRow(fix, "Sky"));     // tracks t1, t2

    QCOMPARE(fix.artwork.readsFor("/music/t1.mp3"), 1);
    QCOMPARE(fix.artwork.readsFor("/music/t2.mp3"), 1);
    QCOMPARE(fix.artwork.readsFor("/music/t3.mp3"), 1);
    QCOMPARE(fix.artwork.readsFor("/music/t4.mp3"), 1);

    // rebuild() rescan may have changed the files → cache cleared.
    fix.browser->rebuild();
    QCOMPARE(fix.artwork.readsFor("/music/t2.mp3"), 2);
}

QTEST_MAIN(TestLibraryCatalogue)
#include "tst_library_catalogue.moc"
