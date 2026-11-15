// SH-1003 Phase B: LibraryCatalogueFacade — the QObject wrapper bridging
// the pure-C++ LibraryCatalogueViewModel to QML for the media browser
// (SH-1005 consumes this surface). Thin: one rebuild invokable, one
// per-mode group query, the discovered playlist paths and the three
// "Unknown …" label constants (the view model decides them; QML never
// hardcodes catalogue strings). All data crosses as plain QVariant
// structures built at this boundary (D3 — the view model deals in plain
// UTF-8 paths and std::string labels only).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "presentation/viewmodels/LibraryCatalogue.h"

namespace shanty {

class LibraryCatalogueFacade : public QObject {
    Q_OBJECT

    /// The discovered playlists (SH-1002 pipeline): flat path list, the
    /// scanner's order verbatim. Empty until the first rebuild().
    Q_PROPERTY(QVariantList playlistPaths READ playlistPaths
                   NOTIFY catalogueChanged)

public:
    explicit LibraryCatalogueFacade(LibraryCatalogueViewModel& viewModel,
                                    QObject* parent = nullptr);

    // ---- queries -------------------------------------------------------------
    /// The groups of one view mode. `mode` is one of the stable QML strings
    /// "artists" | "albums" | "genres" (anything else yields an empty list).
    /// Each group is a QVariantMap:
    ///     { label:   QString,
    ///       paths:   QVariantList of QString (track paths, display-title
    ///                ordered),
    ///       children: QVariantList of the same group shape (artists mode
    ///                two levels — the albums; other modes always empty) }
    /// The lists are recomputed on every call — the view model's data is
    /// only mutated by rebuild(), so QML re-queries after its own
    /// rebuild() (or a changed-library signal from elsewhere).
    Q_INVOKABLE QVariantList groups(const QString& mode) const;

    /// Rescans through the shared pipeline (one store.load + both scanner
    /// queries + one tag read per track) and publishes catalogueChanged.
    Q_INVOKABLE void rebuild();

    /// The view model's verbatim Unknown labels (decision 3) — exposed so
    /// SH-1005's rows and headers never re-decide the strings.
    Q_INVOKABLE QString unknownArtistLabel() const;
    Q_INVOKABLE QString unknownAlbumLabel() const;
    Q_INVOKABLE QString unknownGenreLabel() const;

    // ---- Q_PROPERTY accessors --------------------------------------------------
    QVariantList playlistPaths() const;

signals:
    /// Fired once after each successful rebuild().
    void catalogueChanged();

private:
    static QVariantList groupsOf(
        const std::vector<CatalogueGroup>& groups);
    static QVariantMap groupToMap(const CatalogueGroup& group);

    LibraryCatalogueViewModel& viewModel_;
};

}  // namespace shanty
