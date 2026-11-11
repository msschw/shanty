// SH-E6 Phase B: internal helpers shared by the facades that can open the
// QML FilePickerPage (Library/Playlist/Settings, one reusable page per
// SH-D603-B1). Free functions so the facades stay thin; QtCore-only.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace shanty {

namespace pickers {

/// Directory listing for FilePickerPage: [{ name, path, isDir }],
/// directories first, sorted by name. @param nameFilters suffix filters
/// (e.g. [".m3u", ".m3u8"]); an empty list admits all files. An
/// unreadable/missing directory yields an empty list (no exceptions).
inline QVariantList listDirectory(const QString& path,
                                  const QVariantList& nameFilters) {
    QVariantList result;
    QDir dir(path);
    if (!dir.exists() || !dir.isReadable()) {
        return result;  // documented empty result for unreadable directories
    }

    QStringList suffixes;
    for (const QVariant& filter : nameFilters) {
        const QString suffix = filter.toString();
        if (!suffix.isEmpty()) {
            suffixes.append(suffix);
        }
    }

    const QFileInfoList infos = dir.entryInfoList(
        suffixes, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot |
                      QDir::Readable,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo& info : infos) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), info.fileName());
        item.insert(QStringLiteral("path"), info.absoluteFilePath());
        item.insert(QStringLiteral("isDir"), info.isDir());
        result.append(item);
    }
    return result;
}

/// Home directory (FilePickerPage start point; the page stays
/// facade-agnostic, so the facades that can open it carry the same hook).
inline QString homeDir() { return QDir::homePath(); }

}  // namespace pickers

}  // namespace shanty
