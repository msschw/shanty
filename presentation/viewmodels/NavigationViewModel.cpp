// SH-601 Phase A: NavigationViewModel implementation — the navigation view
// model is a simple LIFO page stack over the three dedicated charter views;
// the full documented decision matrix (push-only-from-root, root-pop no-op,
// optional Player anchor, hook firing rules) lives in NavigationViewModel.h
// and is encoded in tst_navigation. The Silica shell consuming this class is
// the epic owner's Phase B (SH-601 code).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/viewmodels/NavigationViewModel.h"

namespace shanty {

NavigationViewModel::NavigationViewModel()
    : stack_(1, Page::Library) {
    // The platform shell always starts on the bare Library root; the
    // constructor fires no observation hooks (documented initial state).
}

void NavigationViewModel::openSettings() {
    if (stack_.back() != Page::Library) {
        // Documented no-op: a push is valid only from the root — this also
        // rejects Settings → Settings (and Player → Settings).
        invalidate("openSettings: not from Library");
        return;
    }
    stack_.push_back(Page::Settings);  // LIFO push onto [Library]
    notifyChanged();
}

void NavigationViewModel::openPlayer(const std::string& context) {
    if (stack_.back() != Page::Library) {
        // Documented no-op: covers Player → Player AND the sibling swap
        // Settings → Player (siblings navigate via back()-to-Library first).
        invalidate("openPlayer: not from Library");
        return;
    }
    stack_.push_back(Page::Player);
    playerContext_ = context;  // "" when called payloadless
    notifyChanged();
}

void NavigationViewModel::back() {
    if (stack_.size() <= 1) {
        // Documented no-op: the Library root can never be popped off, so a
        // pop (and therefore a double-pop after unwinding) is rejected.
        invalidate("back: already at the root");
        return;
    }
    const Page popped = stack_.back();
    stack_.pop_back();
    if (popped == Page::Player) {
        // The Player anchor lives only while the Player page is up.
        playerContext_.clear();
    }
    notifyChanged();
}

NavigationViewModel::Page NavigationViewModel::currentPage() const {
    return stack_.back();
}

std::size_t NavigationViewModel::depth() const { return stack_.size(); }

bool NavigationViewModel::canGoBack() const { return stack_.size() > 1; }

const std::string& NavigationViewModel::playerContext() const {
    return playerContext_;
}

void NavigationViewModel::invalidate(const char* reason) {
    if (onInvalidNavigation) {
        // Documented no-op: stack/context untouched, reason only observable
        // through the hook.
        onInvalidNavigation(reason);
    }
}

void NavigationViewModel::notifyChanged() {
    if (onNavigationChanged) {
        onNavigationChanged();
    }
}

}  // namespace shanty
