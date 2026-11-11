// SH-601 Phase A: NavigationViewModel page-stack state machine (Library /
// Settings / Player), locked against the decision matrix documented in
// presentation/viewmodels/NavigationViewModel.h:
//   pushes only from the root · Settings→Settings, Player→Player AND the
//   sibling swaps (Settings→Player / Player→Settings) are invalid no-ops ·
//   back() unwinds onto Library, pop/double-pop on the root is invalid ·
//   optional simple string context carried by openPlayer(), dropped on pop.
// Phase A runs against the SH-601 stub: every target case must fail by
// ASSERTION (QFATAL count 0), never by crash.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <cstddef>
#include <string>
#include <vector>

#include "presentation/viewmodels/NavigationViewModel.h"

using shanty::NavigationViewModel;

namespace {

/// Fresh view model + observation logs per test.
struct Fixture {
    NavigationViewModel vm;

    std::vector<std::string> invalidReasons;  ///< onInvalidNavigation log
    std::vector<NavigationViewModel::Page> tops;  ///< top page per change
    std::vector<std::size_t> depths;  ///< stack depth per change

    Fixture() {
        vm.onInvalidNavigation = [this](const char* reason) {
            invalidReasons.push_back(reason ? reason : "(null)");
        };
        vm.onNavigationChanged = [this]() {
            tops.push_back(vm.currentPage());
            depths.push_back(vm.depth());
        };
    }
};

/// Last logged invalid-reason helper keeps the side agnostic.
QString lastReason(const std::vector<std::string>& reasons) {
    return reasons.empty() ? QString()
                           : QString::fromStdString(reasons.back());
}

QString asQ(const std::string& s) { return QString::fromStdString(s); }

}  // namespace

class TestNavigation : public QObject {
    Q_OBJECT

private slots:
    // ---- initial state -----------------------------------------------------
    void initialStackIsBareLibraryRootWithoutNotifications();

    // ---- valid transitions (pushes from the root, back to Library) ---------
    void openSettingsPushesSiblingOntoRoot();
    void openPlayerPushesSiblingWithOptionalContext();
    void openPlaylistPushesSiblingOntoRoot();
    void backReturnsToLibraryFromEverySibling();

    // ---- invalid pushes (sibling swaps and self-pushes) --------------------
    void pushesWhileSiblingOnTopAreInvalidNoops();

    // ---- invalid pops (root pop, double pop) --------------------------------
    void backOnRootIsInvalidNoop();

    // ---- player context lifecycle -------------------------------------------
    void playerContextSurvivesWhilePlayerIsUpAndIsDroppedOnPop();
};

void TestNavigation::initialStackIsBareLibraryRootWithoutNotifications() {
    Fixture f;

    // Documented decision: the stack starts as [Library] root, depth 1, and
    // the constructor fires no hooks.
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.vm.canGoBack(), false);
    QCOMPARE(asQ(f.vm.playerContext()), QString());
    QCOMPARE(f.tops.size(), size_t(0));
    QCOMPARE(f.depths.size(), size_t(0));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::openSettingsPushesSiblingOntoRoot() {
    Fixture f;
    f.vm.openSettings();

    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Settings);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.vm.canGoBack(), true);
    // Exactly one actual transition was exposed to observers.
    QCOMPARE(f.tops.size(), size_t(1));
    QCOMPARE(f.tops.back(), NavigationViewModel::Page::Settings);
    QCOMPARE(f.depths.back(), size_t(2));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::openPlayerPushesSiblingWithOptionalContext() {
    Fixture f;
    f.vm.openPlayer("song:a.mp3");

    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Player);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.vm.canGoBack(), true);
    // The anchor context is carried while the Player page is up.
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:a.mp3"));
    QCOMPARE(f.tops.back(), NavigationViewModel::Page::Player);
    QCOMPARE(f.depths.back(), size_t(2));

    // Documented decision: a payloadless openPlayer() anchors with "".
    Fixture bare;
    bare.vm.openPlayer();
    QCOMPARE(bare.vm.currentPage(), NavigationViewModel::Page::Player);
    QCOMPARE(asQ(bare.vm.playerContext()), QString());
    QCOMPARE(bare.tops.back(), NavigationViewModel::Page::Player);
    QCOMPARE(bare.invalidReasons.size(), size_t(0));
}

void TestNavigation::backReturnsToLibraryFromEverySibling() {
    Fixture f;
    f.vm.openSettings();
    f.vm.back();

    // Settings unwinds onto the Library root.
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.vm.canGoBack(), false);
    QCOMPARE(f.tops.size(), size_t(2));
    QCOMPARE(f.tops.back(), NavigationViewModel::Page::Library);
    QCOMPARE(f.depths.back(), size_t(1));

    // Player unwinds onto the Library root the same way.
    f.vm.openPlayer("song:b.ogg");
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.tops.size(), size_t(4));
    QCOMPARE(f.tops.back(), NavigationViewModel::Page::Library);
    QCOMPARE(f.depths.back(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(0));

    // Playlist (wave-8 follow-up: the SH-603 page needed a navigation
    // path — the original three-page contract predates the checklist's
    // M3U round-trip rows) unwinds onto the Library root the same way.
    f.vm.openPlaylist();
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Playlist);
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.tops.size(), size_t(6));
    QCOMPARE(f.tops.back(), NavigationViewModel::Page::Library);
    QCOMPARE(f.depths.back(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::openPlaylistPushesSiblingOntoRoot() {
    // Same push rule as Settings/Player (one rule, now four siblings):
    // Library → Playlist pushes, exactly one transition, no rejections.
    Fixture f;
    f.vm.openPlaylist();

    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Playlist);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.vm.canGoBack(), true);
    QCOMPARE(f.tops.size(), size_t(1));
    QCOMPARE(f.tops.back(), NavigationViewModel::Page::Playlist);
    QCOMPARE(f.depths.back(), size_t(2));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::pushesWhileSiblingOnTopAreInvalidNoops() {
    // Documented decision (one rule, N consequences): a push is valid
    // only while Library is on top — Settings→Settings, Player→Player,
    // Playlist→Playlist and every sibling swap are no-ops.
    Fixture f;
    f.vm.openSettings();  // sibling now on top

    f.vm.openSettings();  // Settings → Settings
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Settings);
    QCOMPARE(f.vm.depth(), size_t(2));
    f.vm.openPlayer();  // Settings → Player (sibling swap)
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Settings);
    QCOMPARE(f.vm.depth(), size_t(2));
    f.vm.openPlaylist();  // Settings → Playlist (sibling swap)
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Settings);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.invalidReasons.size(), size_t(3));
    QCOMPARE(lastReason(f.invalidReasons), QString("openPlaylist: not from Library"));
    QCOMPARE(f.tops.size(), size_t(1));  // no extra transition notifications

    // The stack is untouched: back() still unwinds the original sibling.
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.tops.size(), size_t(2));

    f.vm.openPlayer("song:c.flac");  // root → Player is valid again
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Player);
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:c.flac"));

    const size_t changed = f.tops.size();  // = 3 (Library top after back)
    f.vm.openPlayer();  // Player → Player
    f.vm.openSettings();  // Player → Settings (sibling swap)
    f.vm.openPlaylist();  // Player → Playlist (sibling swap)
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Player);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.tops.size(), changed);
    QCOMPARE(f.invalidReasons.size(), size_t(6));
    QCOMPARE(lastReason(f.invalidReasons), QString("openPlaylist: not from Library"));

    // Playlist behaves as a sibling of the same rule.
    f.vm.back();
    f.vm.openPlaylist();  // root → Playlist, valid
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Playlist);
    const size_t changed2 = f.tops.size();
    f.vm.openPlaylist();  // Playlist → Playlist
    f.vm.openPlayer("song:d.mp3");  // Playlist → Player (sibling swap)
    f.vm.openSettings();  // Playlist → Settings (sibling swap)
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Playlist);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.tops.size(), changed2);
    QCOMPARE(f.invalidReasons.size(), size_t(9));
    QCOMPARE(lastReason(f.invalidReasons), QString("openSettings: not from Library"));
}

void TestNavigation::backOnRootIsInvalidNoop() {
    Fixture f;

    // Documented decision: popping on the Library root is an invalid no-op.
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("back: already at the root"));
    QCOMPARE(f.tops.size(), size_t(0));

    // Double-pop after unwinding is invalid the same way — Library can
    // never be popped off.
    f.vm.openPlayer();
    f.vm.back();  // valid pop onto the root
    QCOMPARE(f.vm.depth(), size_t(1));
    f.vm.back();  // already at the root
    QCOMPARE(f.vm.currentPage(), NavigationViewModel::Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(2));
    QCOMPARE(lastReason(f.invalidReasons), QString("back: already at the root"));
    QCOMPARE(f.tops.size(), size_t(2));  // only the two actual transitions
}

void TestNavigation::playerContextSurvivesWhilePlayerIsUpAndIsDroppedOnPop() {
    Fixture f;
    f.vm.openPlayer("song:d.mp3");
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:d.mp3"));

    // The context survives as long as the Player page is up (popping a
    // later sibling never existed — the model keeps depth ≤ 2).
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:d.mp3"));

    // Documented decision: popping the Player page drops the anchor.
    f.vm.back();
    QCOMPARE(asQ(f.vm.playerContext()), QString());

    // The root never carries a payload, and Settings pages carry none.
    f.vm.openSettings();
    QCOMPARE(asQ(f.vm.playerContext()), QString());
    f.vm.back();
    QCOMPARE(asQ(f.vm.playerContext()), QString());

    // A fresh push may set a new anchor (old one was already dropped).
    f.vm.openPlayer("song:e.ogg");
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:e.ogg"));
}

QTEST_MAIN(TestNavigation)
#include "tst_navigation.moc"
