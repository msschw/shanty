// SH-801 Phase A: NavigationViewModel implementation — the navigation view
// model is a simple LIFO page stack over the dedicated charter views; the
// full documented decision matrix (stable Library root, show() as the one
// sibling command with atomic sibling replace, root-pop no-op, optional
// Player anchor, landing decision, hook firing rules) lives in
// NavigationViewModel.h and is encoded in tst_navigation. The Silica shell
// consuming this class is the epic owner's Phase B (SH-801 code).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/viewmodels/NavigationViewModel.h"

namespace shanty {

NavigationViewModel::NavigationViewModel()
    : stack_(1, Page::Library) {
    // The platform shell always starts on the bare Library root; the
    // constructor fires no observation hooks (documented initial state).
    // The landing decision (landingPage) is applied by the shell as an
    // explicit startup push — the root itself never mutates.
}

void NavigationViewModel::show(Page page, const std::string& context) {
    (void)page;
    (void)context;
    // SH-801 Phase A stub: the show() contract is locked red-first in
    // tst_navigation; every target case must fail by assertion (with this
    // reason observable through the hook) until Phase B implements the
    // real sibling command.
    invalidate("unimplemented: show");
}

NavigationViewModel::Page NavigationViewModel::landingPage(bool) {
    // SH-801 Phase A stub: the landing decision is not implemented yet —
    // always Library so the hasQueue=true target case fails by assertion.
    return Page::Library;
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

const char* NavigationViewModel::pageName(Page page) {
    // Lowercase facade names (mirrored by NavigationFacade::currentPage);
    // used to build the "show: already showing <page>" catalogue strings.
    switch (page) {
        case Page::Settings:
            return "settings";
        case Page::Player:
            return "player";
        case Page::Playlist:
            return "playlist";
        case Page::Library:
            break;
    }
    return "library";
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
