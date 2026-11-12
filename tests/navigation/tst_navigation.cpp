// SH-801 Phase A: NavigationViewModel page-stack state machine, revised
// navigation contract (UX-1): the Library page is the STABLE root and
// show(page) is the ONE sibling command (root push / atomic sibling
// replace / documented no-ops), replacing the four open* special cases
// and their per-source rejection strings. back() semantics unchanged.
// Landing decision (UX-1): landingPage(bool hasQueue) — the shell pushes
// the Player over the fresh Library root exactly when a queue exists.
// Phase A runs against the SH-801 stub: every target case must fail by
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

using Page = NavigationViewModel::Page;

}  // namespace

class TestNavigation : public QObject {
    Q_OBJECT

private slots:
    // ---- initial state -----------------------------------------------------
    void initialStackIsBareLibraryRootWithoutNotifications();

    // ---- show(): the one sibling command ------------------------------------
    void showFromRootPushesEachSibling();
    void showFromSiblingReplacesTopAtomically();
    void showCurrentPageIsInvalidNoop();
    void showLibraryIsRejectedEverywhere();
    void showPlayerCarriesOptionalContext();
    void showChainsNeverGrowTheStack();

    // ---- back() (unchanged contract) -----------------------------------------
    void backReturnsToLibraryFromEverySibling();
    void backOnRootIsInvalidNoop();

    // ---- landing decision (UX-1) ----------------------------------------------
    void landingPageFollowsQueuePresence();
};

void TestNavigation::initialStackIsBareLibraryRootWithoutNotifications() {
    Fixture f;

    // Documented decision: the stack starts as [Library] root, depth 1, and
    // the constructor fires no hooks — regardless of the landing decision
    // (the player-first push is an explicit startup command).
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.vm.canGoBack(), false);
    QCOMPARE(asQ(f.vm.playerContext()), QString());
    QCOMPARE(f.tops.size(), size_t(0));
    QCOMPARE(f.depths.size(), size_t(0));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::showFromRootPushesEachSibling() {
    // One rule for all three siblings: from the bare root, show(page)
    // pushes (depth 1 → 2), exactly one transition, no rejections.
    const Page siblings[] = {Page::Settings, Page::Player, Page::Playlist};
    for (Page target : siblings) {
        Fixture f;
        f.vm.show(target);

        QCOMPARE(f.vm.currentPage(), target);
        QCOMPARE(f.vm.depth(), size_t(2));
        QCOMPARE(f.vm.canGoBack(), true);
        QCOMPARE(f.tops.size(), size_t(1));
        QCOMPARE(f.tops.back(), target);
        QCOMPARE(f.depths.back(), size_t(2));
        QCOMPARE(f.invalidReasons.size(), size_t(0));
    }
}

void TestNavigation::showFromSiblingReplacesTopAtomically() {
    // Documented decision: show(x) from a sibling is an ATOMIC replace of
    // the top — the stack never exposes an intermediate [Library] state
    // and never grows past depth 2; observers see exactly ONE transition
    // per show() (the QML shell drives pageStack.replace from it).
    const Page siblings[] = {Page::Settings, Page::Player, Page::Playlist};
    for (Page from : siblings) {
        for (Page target : siblings) {
            if (from == target) continue;  // self-switch is its own case

            Fixture f;
            f.vm.show(from);
            QCOMPARE(f.tops.size(), size_t(1));

            f.vm.show(target);
            QCOMPARE(f.vm.currentPage(), target);
            QCOMPARE(f.vm.depth(), size_t(2));  // never deeper than 2
            QCOMPARE(f.vm.canGoBack(), true);
            QCOMPARE(f.tops.size(), size_t(2));  // ONE more transition
            QCOMPARE(f.tops.back(), target);
            QCOMPARE(f.depths.back(), size_t(2));
            QCOMPARE(f.invalidReasons.size(), size_t(0));
        }
    }
}

void TestNavigation::showCurrentPageIsInvalidNoop() {
    // Documented no-op: show(page) while page is already on top changes
    // nothing and fires the invalid hook with the stable catalogue string
    // (lowercase facade names).
    struct Case {
        Page page;
        const char* reason;
    };
    const Case cases[] = {
        {Page::Settings, "show: already showing settings"},
        {Page::Player, "show: already showing player"},
        {Page::Playlist, "show: already showing playlist"},
    };
    for (const Case& c : cases) {
        Fixture f;
        f.vm.show(c.page);
        QCOMPARE(f.tops.size(), size_t(1));

        f.vm.show(c.page);
        QCOMPARE(f.vm.currentPage(), c.page);
        QCOMPARE(f.vm.depth(), size_t(2));
        QCOMPARE(f.tops.size(), size_t(1));  // no extra transition
        QCOMPARE(f.invalidReasons.size(), size_t(1));
        QCOMPARE(lastReason(f.invalidReasons), QString(c.reason));
    }

    // A Player anchor survives a rejected self-show untouched.
    Fixture f;
    f.vm.show(Page::Player, "song:a.mp3");
    f.vm.show(Page::Player);
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:a.mp3"));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons),
             QString("show: already showing player"));
}

void TestNavigation::showLibraryIsRejectedEverywhere() {
    // Documented decision: the Library root is only ever reached through
    // back(); show(Library) is an invalid no-op from the root AND from a
    // sibling.
    Fixture root;
    root.vm.show(Page::Library);
    QCOMPARE(root.vm.currentPage(), Page::Library);
    QCOMPARE(root.vm.depth(), size_t(1));
    QCOMPARE(root.tops.size(), size_t(0));
    QCOMPARE(root.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(root.invalidReasons),
             QString("show: library is the root"));

    Fixture f;
    f.vm.show(Page::Settings);
    QCOMPARE(f.tops.size(), size_t(1));
    f.vm.show(Page::Library);
    // The stack is untouched: the sibling stays up and back() still
    // unwinds onto the Library root afterwards.
    QCOMPARE(f.vm.currentPage(), Page::Settings);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.tops.size(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons),
             QString("show: library is the root"));
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.tops.size(), size_t(2));
}

void TestNavigation::showPlayerCarriesOptionalContext() {
    // The anchor lives only while the Player page is up (unchanged rule,
    // now carried by show()): a payloaded show(Player, ...) anchors the
    // fresh page; leaving the Player drops the anchor; a payloadless
    // show(Player) anchors with "".
    Fixture f;
    f.vm.show(Page::Player, "song:a.mp3");
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:a.mp3"));

    // An atomic replace away from the Player drops the anchor.
    f.vm.show(Page::Settings);
    QCOMPARE(asQ(f.vm.playerContext()), QString());

    // A payloadless show(Player) from a sibling is a fresh page: "".
    f.vm.show(Page::Player);
    QCOMPARE(asQ(f.vm.playerContext()), QString());
    QCOMPARE(f.vm.currentPage(), Page::Player);

    // A payloaded replace onto the Player from a sibling sets the anchor.
    f.vm.show(Page::Playlist);
    f.vm.show(Page::Player, "song:b.ogg");
    QCOMPARE(asQ(f.vm.playerContext()), QString::fromLatin1("song:b.ogg"));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::showChainsNeverGrowTheStack() {
    // LIFO safety under chained sibling switches: an arbitrary cycle of
    // show() commands keeps depth == 2 and fires exactly one transition
    // per switch; back() still unwinds onto the root afterwards.
    Fixture f;
    f.vm.show(Page::Settings);
    f.vm.show(Page::Player);
    f.vm.show(Page::Playlist);
    f.vm.show(Page::Settings);
    f.vm.show(Page::Player);

    QCOMPARE(f.vm.currentPage(), Page::Player);
    QCOMPARE(f.vm.depth(), size_t(2));
    QCOMPARE(f.tops.size(), size_t(5));  // 1 push + 4 switches
    QCOMPARE(f.invalidReasons.size(), size_t(0));

    f.vm.back();
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.tops.size(), size_t(6));
}

void TestNavigation::backReturnsToLibraryFromEverySibling() {
    Fixture f;
    f.vm.show(Page::Settings);
    f.vm.back();

    // Settings unwinds onto the Library root.
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.vm.canGoBack(), false);
    QCOMPARE(f.tops.size(), size_t(2));
    QCOMPARE(f.tops.back(), Page::Library);
    QCOMPARE(f.depths.back(), size_t(1));

    // Player unwinds onto the Library root the same way (and drops its
    // anchor with it).
    f.vm.show(Page::Player, "song:b.ogg");
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(asQ(f.vm.playerContext()), QString());
    QCOMPARE(f.tops.size(), size_t(4));
    QCOMPARE(f.tops.back(), Page::Library);
    QCOMPARE(f.depths.back(), size_t(1));

    // Playlist unwinds onto the Library root the same way.
    f.vm.show(Page::Playlist);
    QCOMPARE(f.vm.currentPage(), Page::Playlist);
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.tops.size(), size_t(6));
    QCOMPARE(f.tops.back(), Page::Library);
    QCOMPARE(f.depths.back(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestNavigation::backOnRootIsInvalidNoop() {
    Fixture f;

    // Documented decision: popping on the Library root is an invalid no-op.
    f.vm.back();
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("back: already at the root"));
    QCOMPARE(f.tops.size(), size_t(0));

    // Double-pop after unwinding is invalid the same way — Library can
    // never be popped off.
    f.vm.show(Page::Player);
    f.vm.back();  // valid pop onto the root
    QCOMPARE(f.vm.depth(), size_t(1));
    f.vm.back();  // already at the root
    QCOMPARE(f.vm.currentPage(), Page::Library);
    QCOMPARE(f.vm.depth(), size_t(1));
    QCOMPARE(f.invalidReasons.size(), size_t(2));
    QCOMPARE(lastReason(f.invalidReasons), QString("back: already at the root"));
    QCOMPARE(f.tops.size(), size_t(2));  // only the two actual transitions
}

void TestNavigation::landingPageFollowsQueuePresence() {
    // Documented decision (UX-1): the landing is the Player exactly when a
    // queue exists at startup (session queue or a restored one); the bare
    // Library root is the landing otherwise (first run / empty state).
    // The initial stack is ALWAYS the bare root — the shell applies the
    // landing as an explicit startup push of the Player page.
    QCOMPARE(NavigationViewModel::landingPage(false), Page::Library);
    QCOMPARE(NavigationViewModel::landingPage(true), Page::Player);
}

QTEST_MAIN(TestNavigation)
#include "tst_navigation.moc"
