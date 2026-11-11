// SH-601 Phase B: NavigationFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "NavigationFacade.h"

namespace shanty {

NavigationFacade::NavigationFacade(NavigationViewModel& viewModel,
                                   QObject* parent)
    : QObject(parent), viewModel_(viewModel) {
    // Mirror the view model's std::function hooks to Qt signals (the wrapper
    // owns/mirrors the callbacks; the view model itself stays Qt-free).
    viewModel_.onNavigationChanged = [this] { emit navigationChanged(); };
    viewModel_.onInvalidNavigation = [this](const char* reason) {
        emit invalidNavigation(QString::fromUtf8(reason));
    };
}

void NavigationFacade::openSettings() { viewModel_.openSettings(); }

void NavigationFacade::openPlayer(const QString& context) {
    viewModel_.openPlayer(context.toStdString());
}

void NavigationFacade::back() { viewModel_.back(); }

QString NavigationFacade::currentPage() const {
    switch (viewModel_.currentPage()) {
        case NavigationViewModel::Page::Settings:
            return QStringLiteral("settings");
        case NavigationViewModel::Page::Player:
            return QStringLiteral("player");
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

}  // namespace shanty
