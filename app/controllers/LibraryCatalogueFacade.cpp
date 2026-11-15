// SH-1003 Phase B: LibraryCatalogueFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryCatalogueFacade.h"

namespace shanty {

LibraryCatalogueFacade::LibraryCatalogueFacade(
    LibraryCatalogueViewModel& viewModel, QObject* parent)
    : QObject(parent), viewModel_(viewModel) {}

QVariantList LibraryCatalogueFacade::groups(const QString& mode) const {
    if (mode == QStringLiteral("artists")) {
        return groupsOf(viewModel_.groups(CatalogueMode::Artists));
    }
    if (mode == QStringLiteral("albums")) {
        return groupsOf(viewModel_.groups(CatalogueMode::Albums));
    }
    if (mode == QStringLiteral("genres")) {
        return groupsOf(viewModel_.groups(CatalogueMode::Genres));
    }
    return {};  // unknown mode string: empty list (no rejection channel)
}

void LibraryCatalogueFacade::rebuild() {
    viewModel_.rebuild();
    emit catalogueChanged();
}

QString LibraryCatalogueFacade::unknownArtistLabel() const {
    return QString::fromUtf8(
        LibraryCatalogueViewModel::unknownArtistLabel());
}

QString LibraryCatalogueFacade::unknownAlbumLabel() const {
    return QString::fromUtf8(LibraryCatalogueViewModel::unknownAlbumLabel());
}

QString LibraryCatalogueFacade::unknownGenreLabel() const {
    return QString::fromUtf8(LibraryCatalogueViewModel::unknownGenreLabel());
}

QVariantList LibraryCatalogueFacade::playlistPaths() const {
    QVariantList list;
    for (const std::string& path : viewModel_.playlistPaths()) {
        list.append(QString::fromStdString(path));
    }
    return list;
}

QVariantList LibraryCatalogueFacade::groupsOf(
    const std::vector<CatalogueGroup>& groups) {
    QVariantList list;
    list.reserve(static_cast<int>(groups.size()));
    for (const CatalogueGroup& group : groups) {
        list.append(groupToMap(group));
    }
    return list;
}

QVariantMap LibraryCatalogueFacade::groupToMap(const CatalogueGroup& group) {
    QVariantMap map;
    map.insert(QStringLiteral("label"), QString::fromStdString(group.label));
    QVariantList paths;
    paths.reserve(static_cast<int>(group.paths.size()));
    for (const std::string& path : group.paths) {
        paths.append(QString::fromStdString(path));
    }
    map.insert(QStringLiteral("paths"), paths);
    map.insert(QStringLiteral("children"), groupsOf(group.children));
    return map;
}

}  // namespace shanty
