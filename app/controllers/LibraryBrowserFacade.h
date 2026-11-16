// SH-1005 Phase B (UX-16): LibraryBrowserFacade — the QObject wrapper
// bridging the pure-C++ LibraryBrowserViewModel to QML for the media
// browser page (LibraryPage).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Thin: the mode selector (stable QML strings "artists" | "albums" |
// "genres" | "playlists"), the current level's rows as plain QVariant
// structures, the openEntry/back drill-down commands, the SH-809
// re-catalogued rowCommands and a rebuild passthrough. All data crosses as
// QVariant at this boundary (D3 — the view model deals in plain UTF-8
// paths and std::string labels only); the QML page holds the row-tap
// bridges (track → PlayerFacade::playFromPaths, playlist →
// PlaylistFacade::requestPlaylistFile — the SH-802 bridge).
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>

#include "presentation/viewmodels/LibraryBrowser.h"

namespace shanty {

class LibraryBrowserFacade : public QObject {
    Q_OBJECT

    /// The current view mode as one of the stable QML strings
    /// "artists" | "albums" | "genres" | "playlists" (initial: "artists").
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    /// The current level's rows as QVariantMaps:
    ///   { kind:  "category" | "track" | "playlist",
    ///     label: QString (category label / display title / playlist path),
    ///     path:  QString (track + playlist rows; "" for category rows) }
    /// Empty until the first rebuild() (the snapshot idiom). Re-queried by
    /// QML on rowsChanged — no per-row notification exists.
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    /// False at the mode root (both group levels of the Artists chain are
    /// "backable"); otherwise true. A property (NOT an invokable) so the
    /// QML back affordance's visible binding re-evaluates on rowsChanged —
    /// a plain invokable call in a binding would never refresh.
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY rowsChanged)

public:
    explicit LibraryBrowserFacade(LibraryBrowserViewModel& viewModel,
                                  QObject* parent = nullptr);

    // ---- navigation (B1/B4/B5 contracts, tests/library_catalogue) ------------
    /// Drills one level down from the row `index` (valid category rows
    /// only). Out of range → invalidCommand("openEntry: index out of
    /// range"), a documented rejection — NOT a silent no-op.
    Q_INVOKABLE void openEntry(int index);
    /// One level up; at the mode root a silent no-op (nothing to reject).
    Q_INVOKABLE void back();

    // ---- SH-809 (UX-8) browser-row catalogue (B6) ------------------------------
    /// The pressAndHold menu commands for the row `index`, in stable QML
    /// string form (mirror of the view model's re-catalogue, which reuses
    /// the LibraryViewModel strings): category rows → [] (nothing to act
    /// on), track rows → ["play", "addToQueue"], playlist rows → ["play"].
    /// Out of range → [] (the "do not open" signal).
    Q_INVOKABLE QVariantList rowCommands(int index) const;

    // ---- rebuild (B7: passthrough + navigation reset) --------------------------
    /// Rescans through the shared catalogue pipeline (one store.load + both
    /// scanner queries + one tag read per track) and resets the drill-down.
    Q_INVOKABLE void rebuild();

    // ---- Q_PROPERTY accessors ------------------------------------------------
    QString mode() const;
    void setMode(const QString& mode);
    QVariantList rows() const;

signals:
    /// Fired after every setMode / openEntry / back / rebuild — the row
    /// level and the navigation state may all have changed.
    void rowsChanged();
    /// Fired after every successful setMode.
    void modeChanged();
    /// Rejected command (view-model onInvalid mirror; the shell toasts it).
    void invalidCommand(const QString& reason);

private:
    static QString modeToString(BrowserMode mode);
    /// Maps a stable QML string to the enum; false for unknown strings
    /// (setMode rejects them — a QML-side programming error surfaces as a
    /// documented rejection, never a silent no-op).
    static bool modeFromString(const QString& mode, BrowserMode* out);
    static QVariantMap rowToMap(const LibraryBrowserViewModel::Row& row);

    LibraryBrowserViewModel& viewModel_;
};

}  // namespace shanty
