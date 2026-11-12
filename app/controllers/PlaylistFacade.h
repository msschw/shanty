// SH-603 Phase B: PlaylistFacade — the QObject wrapper bridging the pure-C++
// PlaylistViewModel (import/export flow state machine + SH-304 mutation
// delegates) to QML. Thin: every view-model hook is mirrored to a Qt signal;
// flow results (Done/Failed + lastError) drive the shell's toasts.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// SH-D603-B1 (import/export UI flow, decision recorded here): the file
// dialogs are the app's own Silica browser page (qml/pages/FilePickerPage.qml)
// instead of ContentPeerPicker/Sailfish.Pickers. Rationale: Shanty plays and
// exports LOCAL files only (charter), ContentPeerPicker is a
// content-transfer-peer picker (overkill and transfer-oriented), and the
// Sailfish.Pickers FilePickerPage cannot act as a save-target dialog. A
// simple directory browser with an optional file-name field covers
// import (open), export (save) and folder picking with one reusable page.
// Directory listing I/O stays on the C++ side (QtCore in this facade) so the
// QML page stays file-system-agnostic and the same page serves all pickers.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "presentation/viewmodels/PlaylistViewModel.h"

namespace shanty {

class PlaylistFacade : public QObject {
    Q_OBJECT

    /// Playlist entries as QVariantMaps:
    ///   { path: QString, title: QString, duration: int }  (duration -1 when
    ///   the playlist carries none — Track::kUnknownDuration).
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY entriesChanged)
    Q_PROPERTY(long long totalDurationSeconds READ totalDurationSeconds
                   NOTIFY entriesChanged)
    /// "idle" | "awaitingFile" | "importing" | "exporting" | "done" |
    /// "failed" (PlaylistFlowState mapped for QML).
    Q_PROPERTY(QString flowState READ flowState NOTIFY flowChanged)
    /// Service error message of the last failed flow run; "" when none.
    Q_PROPERTY(QString lastError READ lastError NOTIFY flowChanged)

public:
    explicit PlaylistFacade(PlaylistViewModel& viewModel,
                            QObject* parent = nullptr);

    // ---- import/export flow (view-model contract, PlaylistViewModel.h) ----
    // Rejections surface through invalidCommand(reason) with the view
    // model's catalogue strings; flow results through flowChanged.
    Q_INVOKABLE void beginImport();
    Q_INVOKABLE void beginExport();
    Q_INVOKABLE void confirmImport(const QString& filePath);
    Q_INVOKABLE void confirmExport(const QString& filePath);
    Q_INVOKABLE void cancelFlow();

    // ---- SH-304 mutation delegates (rejections are domain no-ops; each
    //      effective change re-emits entriesChanged via the service hook) --
    Q_INVOKABLE void addEntry(const QString& path, const QString& title
                                                  = QString(),
                              int durationSeconds = -1);
    Q_INVOKABLE void moveEntry(int from, int to);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void deduplicate();

    // ---- SH-802 (UX-2): the "Play" bridge emitter ---------------------------
    /// Extracts the entry paths C++-side (no QML logic) and emits
    /// playRequested(paths) — the app wiring routes it to the player side's
    /// queue-replace play command (PlayerFacade::playFromPaths), whose
    /// empty-check is the single decision point. The EMPTY-PLAYLIST
    /// rejection surfaces on the player side; this emitter carries no
    /// decision of its own.
    Q_INVOKABLE void requestPlay();

    // ---- file-dialog support (SH-D603-B1; QtCore side of the browsing) ----
    /// Directory listing for FilePickerPage: [{ name, path, isDir }],
    /// directories first, sorted by name. @param nameFilters suffix filters
    /// (e.g. [".m3u", ".m3u8"]); an empty list admits all files.
    /// An unreadable/missing directory yields an empty list (no exceptions).
    Q_INVOKABLE QVariantList listDirectory(const QString& path,
                                           const QVariantList& nameFilters)
        const;
    /// Home directory (FilePickerPage start point; same hook as on the
    /// Library/Settings facades so the picker stays facade-agnostic).
    Q_INVOKABLE QString homeDir() const;

    // ---- Q_PROPERTY accessors ------------------------------------------------
    QVariantList entries() const;
    int entryCount() const;
    long long totalDurationSeconds() const;
    QString flowState() const;
    QString lastError() const;

signals:
    /// Fired after every actual flow-state transition (view-model mirror).
    void flowChanged();
    /// Fired once per service change event; the entries are already updated
    /// at callback time (ordering guarantee preserved).
    void entriesChanged();
    /// Rejected flow command (view-model onInvalidCommand mirror).
    void invalidCommand(const QString& reason);
    /// SH-802: the "Play" request with the extracted entry paths (playlist
    /// order); the app wiring routes it to PlayerFacade::playFromPaths.
    void playRequested(const QVariantList& paths);

private:
    PlaylistViewModel& viewModel_;
};

}  // namespace shanty
