// SH-1005 Phase B (UX-16): LibraryBrowserFacade implementation (see the
// header). The facade emits rowsChanged after every state-changing call —
// the view model has no observer of its own (pure C++, no Qt), so the QML
// side re-queries the snapshot getters on the signal.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryBrowserFacade.h"

namespace shanty {

LibraryBrowserFacade::LibraryBrowserFacade(LibraryBrowserViewModel& viewModel,
                                           QObject* parent)
    : QObject(parent), viewModel_(viewModel) {
    viewModel_.onInvalid = [this](const char* reason) {
        emit invalidCommand(QString::fromUtf8(reason));
    };
}

void LibraryBrowserFacade::openEntry(int index) {
    if (index < 0) {
        // Out of range is a documented rejection; negative values never
        // reach the unsigned domain API.
        emit invalidCommand(QStringLiteral("openEntry: index out of range"));
        return;
    }
    viewModel_.openEntry(static_cast<std::size_t>(index));
    emit rowsChanged();
}

void LibraryBrowserFacade::back() {
    viewModel_.back();
    emit rowsChanged();
}

bool LibraryBrowserFacade::canGoBack() const { return viewModel_.canGoBack(); }

QVariantList LibraryBrowserFacade::rowCommands(int index) const {
    if (index < 0) {
        return {};
    }
    QVariantList commands;
    for (const LibraryViewModel::RowCommand command :
         viewModel_.rowCommands(static_cast<std::size_t>(index))) {
        switch (command) {
            case LibraryViewModel::RowCommand::Play:
                commands.append(QStringLiteral("play"));
                break;
            case LibraryViewModel::RowCommand::AddToQueue:
                commands.append(QStringLiteral("addToQueue"));
                break;
        }
    }
    return commands;
}

void LibraryBrowserFacade::rebuild() {
    viewModel_.rebuild();
    emit rowsChanged();
}

QString LibraryBrowserFacade::mode() const {
    return modeToString(viewModel_.mode());
}

void LibraryBrowserFacade::setMode(const QString& mode) {
    BrowserMode parsed;
    if (!modeFromString(mode, &parsed)) {
        emit invalidCommand(QStringLiteral("setMode: unknown mode"));
        return;
    }
    viewModel_.setMode(parsed);
    emit modeChanged();
    emit rowsChanged();
}

QVariantList LibraryBrowserFacade::rows() const {
    QVariantList list;
    const std::vector<LibraryBrowserViewModel::Row>& rows = viewModel_.rows();
    list.reserve(static_cast<int>(rows.size()));
    for (const LibraryBrowserViewModel::Row& row : rows) {
        list.append(rowToMap(row));
    }
    return list;
}

QString LibraryBrowserFacade::modeToString(BrowserMode mode) {
    switch (mode) {
        case BrowserMode::Albums:
            return QStringLiteral("albums");
        case BrowserMode::Genres:
            return QStringLiteral("genres");
        case BrowserMode::Playlists:
            return QStringLiteral("playlists");
        case BrowserMode::Artists:
            break;
    }
    return QStringLiteral("artists");
}

bool LibraryBrowserFacade::modeFromString(const QString& mode,
                                          BrowserMode* out) {
    if (mode == QStringLiteral("artists")) {
        *out = BrowserMode::Artists;
    } else if (mode == QStringLiteral("albums")) {
        *out = BrowserMode::Albums;
    } else if (mode == QStringLiteral("genres")) {
        *out = BrowserMode::Genres;
    } else if (mode == QStringLiteral("playlists")) {
        *out = BrowserMode::Playlists;
    } else {
        return false;
    }
    return true;
}

QVariantMap LibraryBrowserFacade::rowToMap(
    const LibraryBrowserViewModel::Row& row) {
    QVariantMap map;
    switch (row.kind) {
        case LibraryBrowserViewModel::Row::Kind::Track:
            map.insert(QStringLiteral("kind"), QStringLiteral("track"));
            break;
        case LibraryBrowserViewModel::Row::Kind::Playlist:
            map.insert(QStringLiteral("kind"), QStringLiteral("playlist"));
            break;
        case LibraryBrowserViewModel::Row::Kind::Category:
            map.insert(QStringLiteral("kind"), QStringLiteral("category"));
            break;
    }
    map.insert(QStringLiteral("label"), QString::fromStdString(row.label));
    map.insert(QStringLiteral("path"), QString::fromStdString(row.path));
    return map;
}

}  // namespace shanty
