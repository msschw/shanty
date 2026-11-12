// SH-601 Phase A (logic part): navigation view-model state machine for the
// application shell. Pure C++ (D1 — no Qt, no exceptions; std::function
// observation hooks, same style as core/playback/PlaybackController.h).
// The Silica side (app/, qml/ page stack, cover page) wraps/exposes this
// class via a QObject facade.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Model: a single simple LIFO page stack over the dedicated views of the
// charter (PROJECT.md) — pages Library (stable root), Settings, Player,
// Playlist.
//
// Documented decisions (SH-801 revision of the SH-601 contract; every one
// locked by tst_navigation):
//   - The Library page is the STABLE root. The initial stack is ALWAYS
//     [Library] (depth 1) and the constructor fires no hooks. The
//     "player-first" start (UX.md UX-1) is an explicit startup push of the
//     Player page over the fresh root (stack [Library, Player]), decided
//     once by landingPage() — the root itself never mutates.
//   - show(page) is the ONE sibling command (SH-801 / UX-1: replaces the
//     four open* special cases; UX-2 and the UX-9 overlay both route
//     through it, valid from any sibling):
//       * from the root: pushes the sibling (depth 1 → 2).
//       * from another sibling: ATOMIC replace of the top — the stack
//         never exposes an intermediate [Library] state and never grows
//         past depth 2; observers see exactly ONE onNavigationChanged
//         fire per show() (the QML shell drives pageStack.replace from
//         it; the per-page back-sync stays race-free because it only
//         fires back() while the VM still shows that page on top).
//       * currentPage() == page: invalid no-op with reason
//         "show: already showing <page>" (<page>: lowercase facade name).
//       * page == Library: invalid no-op EVERYWHERE ("show: library is
//         the root") — the root is only ever reached through back().
//   - back() pops the top sibling; valid only when depth() > 1. Popping on
//     the Library root (and therefore a double-pop after unwinding) is the
//     same kind of invalid no-op ("back: already at the root") with NO state
//     change — Library can never be popped off.
//   - Player context: show(Page::Player, context) may carry a simple string
//     anchor for the Player page ("" when none — the SH-601 "no payload or a
//     simple string" minimum, e.g. a song id). It is readable through
//     playerContext() while the Player page is up and is dropped when the
//     Player page goes away (back() or an atomic show() replace); the root
//     never carries a payload.
//   - onNavigationChanged() fires ONLY on actual transitions (push, pop,
//     atomic sibling replace — one fire each); rejected no-ops leave the
//     stack untouched and fire nothing except onInvalidNavigation.
//   - landingPage(hasQueue) (SH-801 / UX-1): pure static decision — Player
//     exactly when a queue exists at startup (session queue, or a restored
//     one once queue persistence lands), Library otherwise (first run /
//     empty state). No state, no hooks.
//   - Invalid-command reason catalogue (exact stable strings asserted in
//     the suite, kept stable for the QML layer):
//         "show: already showing player"
//         "show: already showing settings"
//         "show: already showing playlist"
//         "show: library is the root"
//         "back: already at the root"
//     Removed with SH-801: the open* commands and their
//     "openPlayer/openPlaylist/openSettings: not from Library" strings —
//     their invalid cases inverted into show() no-ops and valid switches.
//     (Wave-8c context: the facade must keep being registered as
//     navigationFacade — Silica injects a per-page context property named
//     "navigation" into every pushed page.)
//   - Wave-8 history (superseded by the show() revision): the original
//     SH-601 contract allowed pushes only from the root with per-command
//     rejection strings, and openPlaylist() (wave-8 follow-up) had added
//     the fourth sibling. The stable-root rule and back() semantics are
//     unchanged; sibling switching is now show()'s job.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace shanty {

class NavigationViewModel {
public:
    /// The dedicated views of the charter; Library is the stable root.
    enum class Page { Library, Settings, Player, Playlist };

    NavigationViewModel();

    // ---- observation hooks -------------------------------------------------
    /// Fired after every actual push/pop/atomic sibling replace (not after
    /// rejected commands).
    std::function<void()> onNavigationChanged;
    /// Fired for a navigation command rejected as a documented no-op.
    std::function<void(const char* reason)> onInvalidNavigation;

    // ---- navigation commands ----------------------------------------------
    /// The ONE sibling command (SH-801 / UX-1): pushes the sibling when the
    /// Library root is on top, atomically replaces the top sibling otherwise;
    /// invalid no-ops per the catalogue above. The optional context anchors
    /// the Player page ("" when none).
    void show(Page page, const std::string& context = std::string());

    /// Pops the top sibling; invalid no-op on the Library root.
    void back();

    // ---- observable state --------------------------------------------------
    Page currentPage() const;
    /// Stack depth; 1 = bare root (no sibling page on top).
    std::size_t depth() const;
    /// True when back() is a valid command (a sibling sits on the root).
    bool canGoBack() const;
    /// Player anchor string ("" when none) of the Player page while it is
    /// up; cleared when the Player page goes away (back() or replace).
    const std::string& playerContext() const;

    // ---- startup landing decision (SH-801 / UX-1) ---------------------------
    /// Pure static decision: Player exactly when a queue exists at startup
    /// (session queue, or a restored one), Library otherwise. The initial
    /// stack is always the bare root; the shell applies this decision as an
    /// explicit startup push of the Player page over the root.
    static Page landingPage(bool hasQueue);

private:
    std::vector<Page> stack_;
    std::string playerContext_;

    static const char* pageName(Page page);

    void invalidate(const char* reason);
    void notifyChanged();
};

}  // namespace shanty
