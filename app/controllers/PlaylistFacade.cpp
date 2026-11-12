// SH-603 Phase B: PlaylistFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaylistFacade.h"

#include <QDir>
#include <QFileInfo>

#include "PickerSupport.h"

namespace shanty {

PlaylistFacade::PlaylistFacade(PlaylistViewModel& viewModel, QObject* parent)
    : QObject(parent), viewModel_(viewModel) {
    // Mirror the view model's std::function hooks to Qt signals.
    viewModel_.onFlowChanged = [this] { emit flowChanged(); };
    viewModel_.onEntriesChanged = [this] { emit entriesChanged(); };
    viewModel_.onInvalidCommand = [this](const char* reason) {
        emit invalidCommand(QString::fromUtf8(reason));
    };
}

void PlaylistFacade::beginImport() { viewModel_.beginImport(); }

void PlaylistFacade::beginExport() { viewModel_.beginExport(); }

void PlaylistFacade::confirmImport(const QString& filePath) {
    viewModel_.confirmImport(filePath.toStdString());
}

void PlaylistFacade::confirmExport(const QString& filePath) {
    viewModel_.confirmExport(filePath.toStdString());
}

void PlaylistFacade::confirmDiscard() { viewModel_.confirmDiscard(); }

void PlaylistFacade::cancelFlow() { viewModel_.cancelFlow(); }

void PlaylistFacade::addEntry(const QString& path, const QString& title,
                              int durationSeconds) {
    PlaylistEntry entry;
    entry.path = path.toStdString();
    entry.title = title.toStdString();
    entry.durationSeconds = durationSeconds;
    viewModel_.add(entry);
}

void PlaylistFacade::moveEntry(int from, int to) {
    if (from < 0 || to < 0) {
        // Out of range is a documented domain no-op; negative values never
        // reach the unsigned domain API.
        return;
    }
    viewModel_.moveEntry(static_cast<std::size_t>(from),
                         static_cast<std::size_t>(to));
}

void PlaylistFacade::removeAt(int index) {
    if (index < 0) {
        return;  // out of range is a documented domain no-op
    }
    viewModel_.removeAt(static_cast<std::size_t>(index));
}

void PlaylistFacade::clear() { viewModel_.clear(); }

void PlaylistFacade::deduplicate() { viewModel_.deduplicate(); }

QVariantList PlaylistFacade::listDirectory(const QString& path,
                                           const QVariantList& nameFilters)
    const {
    return pickers::listDirectory(path, nameFilters);
}

QString PlaylistFacade::homeDir() const { return pickers::homeDir(); }

QVariantList PlaylistFacade::entries() const {
    QVariantList list;
    const std::vector<PlaylistEntry>& entries = viewModel_.entries();
    list.reserve(static_cast<int>(entries.size()));
    for (const PlaylistEntry& entry : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("path"),
                    QString::fromStdString(entry.path));
        item.insert(QStringLiteral("title"),
                    QString::fromStdString(entry.title));
        item.insert(QStringLiteral("duration"), entry.durationSeconds);
        list.append(item);
    }
    return list;
}

int PlaylistFacade::entryCount() const {
    return static_cast<int>(viewModel_.entryCount());
}

void PlaylistFacade::requestPlay() {
    // SH-802: extract the paths C++-side (playlist order) and hand them to
    // the app wiring; the player side owns the empty-check decision.
    QVariantList paths;
    const std::vector<PlaylistEntry>& entries = viewModel_.entries();
    paths.reserve(static_cast<int>(entries.size()));
    for (const PlaylistEntry& entry : entries) {
        paths.append(QString::fromStdString(entry.path));
    }
    emit playRequested(paths);
}

long long PlaylistFacade::totalDurationSeconds() const {
    return viewModel_.totalDurationSeconds();
}

QString PlaylistFacade::flowState() const {
    switch (viewModel_.flowState()) {
        case PlaylistFlowState::AwaitingConfirm:
            return QStringLiteral("awaitingConfirm");
        case PlaylistFlowState::AwaitingFile:
            return QStringLiteral("awaitingFile");
        case PlaylistFlowState::Importing:
            return QStringLiteral("importing");
        case PlaylistFlowState::Exporting:
            return QStringLiteral("exporting");
        case PlaylistFlowState::Done:
            return QStringLiteral("done");
        case PlaylistFlowState::Failed:
            return QStringLiteral("failed");
        case PlaylistFlowState::Idle:
            break;
    }
    return QStringLiteral("idle");
}

QString PlaylistFacade::lastError() const {
    return QString::fromStdString(viewModel_.lastError());
}

}  // namespace shanty
