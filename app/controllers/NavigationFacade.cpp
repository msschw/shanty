// SH-601 Phase B / SH-801: NavigationFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "NavigationFacade.h"

namespace shanty {

NavigationFacade::NavigationFacade(NavigationViewModel& viewModel,
                                   bool landingHasQueue, QObject* parent)
    : QObject(parent), viewModel_(viewModel), landingHasQueue_(landingHasQueue) {
    // Mirror the view model's std::function hooks to Qt signals (the wrapper
    // owns/mirrors the callbacks; the view model itself stays Qt-free).
    viewModel_.onNavigationChanged = [this] { emit navigationChanged(); };
    viewModel_.onInvalidNavigation = [this](const char* reason) {
        emit invalidNavigation(QString::fromUtf8(reason));
    };
}

void NavigationFacade::show(const QString& page, const QString& context) {
    // Thin name forward; unknown names are a facade-level guard (malformed
    // QML call site) — the view model's command domain is the three
    // siblings, "library" is reachable only through back().
    const std::string name = page.toStdString();
    NavigationViewModel::Page target;
    if (name == "settings") {
        target = NavigationViewModel::Page::Settings;
    } else if (name == "player") {
        target = NavigationViewModel::Page::Player;
    } else if (name == "playlist") {
        target = NavigationViewModel::Page::Playlist;
    } else {
        emit invalidNavigation(QStringLiteral("show: unknown page"));
        return;
    }
    viewModel_.show(target, context.toStdString());
}

void NavigationFacade::back() { viewModel_.back(); }

QString NavigationFacade::currentPage() const {
    switch (viewModel_.currentPage()) {
        case NavigationViewModel::Page::Settings:
            return QStringLiteral("settings");
        case NavigationViewModel::Page::Player:
            return QStringLiteral("player");
        case NavigationViewModel::Page::Playlist:
            return QStringLiteral("playlist");
        case NavigationViewModel::Page::Library:
            break;
    }
    return QStringLiteral("library");
}

int NavigationFacade::depth() const {
    return static_cast<int>(viewModel_.depth());
}

bool NavigationFacade::canGoBack() const { return viewModel_.canGoBack(); }

QString NavigationFacade::playerContext() const {
    return QString::fromStdString(viewModel_.playerContext());
}

QString NavigationFacade::landing() const {
    return NavigationViewModel::landingPage(landingHasQueue_) ==
                   NavigationViewModel::Page::Player
               ? QStringLiteral("player")
               : QStringLiteral("library");
}

}  // namespace shanty
