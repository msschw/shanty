// SH-602 Phase B: LibraryFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryFacade.h"

#include "PickerSupport.h"

namespace shanty {

LibraryFacade::LibraryFacade(LibraryViewModel& viewModel, QObject* parent)
    : QObject(parent), viewModel_(viewModel) {
    // Mirror the view model's hooks; onInvalid fires for rejections only,
    // onConfigChanged exactly once per effective persisted mutation.
    viewModel_.onInvalid = [this](const char* reason) {
        emit invalidReason(QString::fromUtf8(reason));
    };
    viewModel_.onConfigChanged = [this] { emit changed(); };
}

void LibraryFacade::addFolder(const QString& path) {
    viewModel_.addFolder(path.toStdString());
}

void LibraryFacade::removeFolder(const QString& path) {
    viewModel_.removeFolder(path.toStdString());
}

void LibraryFacade::addWhitelistEntry(const QString& path) {
    viewModel_.addWhitelistEntry(path.toStdString());
}

void LibraryFacade::removeWhitelistEntry(const QString& path) {
    viewModel_.removeWhitelistEntry(path.toStdString());
}

void LibraryFacade::addBlacklistEntry(const QString& path) {
    viewModel_.addBlacklistEntry(path.toStdString());
}

void LibraryFacade::removeBlacklistEntry(const QString& path) {
    viewModel_.removeBlacklistEntry(path.toStdString());
}

void LibraryFacade::reload() { viewModel_.reload(); }

QVariantList LibraryFacade::scan() {
    const std::vector<std::string> scanned = viewModel_.scan();
    // scan() resets the selection and replaces the listing; the view model
    // fires no event for it, so this facade publishes the refresh itself.
    emit changed();
    return toStringList(scanned);
}

void LibraryFacade::selectAll() {
    viewModel_.selectAll();
    emit changed();  // selection changes carry no view-model event (V8)
}

void LibraryFacade::clearSelection() {
    viewModel_.clearSelection();
    emit changed();
}

void LibraryFacade::setSelected(int index, bool selected) {
    if (index < 0) {
        // Mirror the view model's catalogue reason for negative indices
        // (the view model takes an unsigned size).
        emit invalidReason(QStringLiteral("setSelected: index out of range"));
        return;
    }
    viewModel_.setSelected(static_cast<std::size_t>(index), selected);
    emit changed();
}

QVariantList LibraryFacade::selectionPaths() {
    return toStringList(viewModel_.selectionPaths());
}

QVariantList LibraryFacade::rowCommands(int index) const {
    // SH-809: mirror the view-model catalogue in stable QML strings; a
    // negative index is out of range like any oversized one (the view
    // model takes std::size_t).
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

QString LibraryFacade::homeDir() const {
    return pickers::homeDir();
}

QVariantList LibraryFacade::listDirectory(const QString& path,
                                          const QVariantList& nameFilters)
    const {
    return pickers::listDirectory(path, nameFilters);
}

QVariantList LibraryFacade::folders() const {
    return toStringList(viewModel_.config().folders);
}

QVariantList LibraryFacade::whitelist() const {
    return toStringList(viewModel_.config().whitelist);
}

QVariantList LibraryFacade::blacklist() const {
    return toStringList(viewModel_.config().blacklist);
}

QVariantList LibraryFacade::tracks() const {
    return toStringList(viewModel_.tracks());
}

QVariantList LibraryFacade::selection() const {
    QVariantList indices;
    for (std::size_t index : viewModel_.selection()) {
        indices.append(static_cast<int>(index));
    }
    return indices;
}

QVariantList LibraryFacade::toStringList(
    const std::vector<std::string>& paths) {
    QVariantList list;
    list.reserve(static_cast<int>(paths.size()));
    for (const std::string& path : paths) {
        list.append(QString::fromStdString(path));
    }
    return list;
}

}  // namespace shanty
