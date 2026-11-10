// SH-601 Phase A (logic part): navigation view-model state machine for the
// application shell. Pure C++ (D1 — no Qt, no exceptions; std::function
// observation hooks, same style as core/playback/PlaybackController.h).
// The Silica side (app/, qml/ page stack, cover page) is the epic owner's
// Phase B and will wrap/expose this class via a QObject facade later.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Model: a single simple LIFO page stack over the three dedicated views of
// the charter (PROJECT.md) — pages Library (root), Settings, Player.
//
// Documented decisions (SH-601; every one locked by tst_navigation):
//   - Pushes happen only FROM the root. openSettings()/openPlayer(...) are
//     valid only while the stack top is Library; a push while a sibling
//     (Settings or Player) is already on top is a documented invalid no-op
//     surfaced via onInvalidNavigation. This marks the state machine as
//     ONE decision rule that simultaneously covers
//       Settings → Settings, Player → Player,
//       Settings → Player, Player → Settings            (all invalid).
//     Rationale: Silica's pageStack is a plain LIFO without sibling-replace
//     semantics; keeping the depth bounded at 2 makes pop() unambiguous
//     (back() always unwinds onto the Library root) and LIFO-safe. Siblings
//     switch views via back()-to-Library first, then push.
//   - back() pops the top sibling; valid only when depth() > 1. Popping on
//     the Library root (and therefore a double-pop after unwinding) is the
//     same kind of invalid no-op ("back: already at the root") with NO state
//     change — Library can never be popped off.
//   - Player context: openPlayer() may carry a simple string anchor for the
//     Player page ("" when none — the ticket's "no payload or a simple
//     string" minimum, e.g. a song/profile id). It is readable through
//     playerContext() while the Player page is up and is dropped when that
//     page is popped (back()); the root never carries a payload.
//   - onNavigationChanged() fires ONLY on actual transitions (push/pop);
//     rejected no-ops leave the stack untouched and fire nothing except
//     onInvalidNavigation. Initial stack is [Library] (depth 1) and the
//     constructor fires no hooks.
//   - Invalid-command reason catalogue (exact stable strings asserted in
//     the suite, kept stable for the QML layer):
//         "openSettings: not from Library"
//         "openPlayer: not from Library"
//         "back: already at the root"
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace shanty {

class NavigationViewModel {
public:
    /// The dedicated views of the charter; Library is the root page.
    enum class Page { Library, Settings, Player };

    NavigationViewModel();

    // ---- observation hooks -------------------------------------------------
    /// Fired after every actual push/pop (not after rejected commands).
    std::function<void()> onNavigationChanged;
    /// Fired for a navigation command rejected as a documented no-op.
    std::function<void(const char* reason)> onInvalidNavigation;

    // ---- navigation commands ----------------------------------------------
    /// Library → Settings (push); invalid no-op unless Library is on top.
    void openSettings();
    /// Library → Player (push); invalid no-op unless Library is on top.
    void openPlayer(const std::string& context = std::string());

    /// Pops the top sibling; invalid no-op on the Library root.
    void back();

    // ---- observable state --------------------------------------------------
    Page currentPage() const;
    /// Stack depth; 1 = bare root (no sibling page on top).
    std::size_t depth() const;
    /// True when back() is a valid command (a sibling sits on the root).
    bool canGoBack() const;
    /// Player anchor string ("" when none) of the Player page while it is
    /// up; cleared when the Player page is popped.
    const std::string& playerContext() const;

private:
    std::vector<Page> stack_;
    std::string playerContext_;

    void invalidate(const char* reason);
    void notifyChanged();
};

}  // namespace shanty
