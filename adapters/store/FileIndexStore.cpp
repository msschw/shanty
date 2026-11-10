// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "FileIndexStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <set>
#include <utility>

namespace shanty {

// ---------------------------------------------------------------------------
// Storage location (SH-204 design, counterpart of the D5 record):
//
//   1. If the env var SHANTY_LIBRARY_STORE is set, it is used verbatim as the
//      absolute target file path. This is a documented hook for tests and
//      development (point a host instance at a scratch file); unset it on
//      the device.
//
//   2. Otherwise the file lives at
//        <QStandardPaths::writableLocation(GenericConfigLocation)> + "/shanty/library.conf"
//      i.e. ~/.config/shanty/library.conf on a plain host. On Sailfish the
//      app runs sandboxed, so this maps to the app's own config directory
//      inside its sandbox (e.g. /home/nemo/.config/<appname>/sandboxed
//      shanty/library.conf of the invoker property) — the app's config
//      location as required, no absolute API paths hardcoded, no writes
//      outside the sandbox.
//
// Non-pollution on the host: only a single `shanty/library.conf` under the
// user's config directory is created (tests read/write the default
// location); nothing is placed into the build tree.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// File format ("shanty library configuration", version 1, plain UTF-8 text):
//
//   # shanty library configuration v1
//   folders=/music
//   folders=/podcast-archiv
//   whitelist=/music/fav
//   blacklist=/music/skip
//
// - One "tag=value" line per entry: the first '=' separates tag and value;
//   ';' is not used so paths may contain any characters, including '='.
// - Groups are always serialized in the fixed order folders -> whitelist ->
//   blacklist and within a group in config order, so equal configs produce
//   byte-identical files (stable ordering).
// - Comments ('#') and blank lines are ignored; unknown tags (future
//   versions) are ignored on load. Data round trips exactly through
//   LibraryConfig::operator== (lists are order-preserving).
// ---------------------------------------------------------------------------

namespace {

// One shared record of the files written by ANY FileIndexStore inside this
// process session. A load() from an instance that did not save itself must
// not adopt same-session data (a fresh store is a fresh library), while data
// from a previous process/app instance is real persistence and is adopted.
std::set<std::string>& sessionWrittenPaths() {
    static std::set<std::string> paths;
    return paths;
}

std::string defaultStorePath() {
    const QString overridePath = qEnvironmentVariable("SHANTY_LIBRARY_STORE");
    if (!overridePath.isEmpty()) {
        return overridePath.toStdString();
    }
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    if (base.isEmpty()) {
        // No config location available (e.g. exotic sandbox setups): fall
        // back to the per-user temp dir rather than a relative path in the
        // current working directory, which could pollute the build tree.
        base = QDir::tempPath();
    }
    return (base + "/shanty/library.conf").toStdString();
}

// Serialize as documented above. Deterministic, byte-identical for equal
// configs.
std::string serialize(const LibraryConfig& config) {
    std::string out;
    out += "# shanty library configuration v1\n";
    for (const std::string& entry : config.folders) {
        out += "folders=" + entry + "\n";
    }
    for (const std::string& entry : config.whitelist) {
        out += "whitelist=" + entry + "\n";
    }
    for (const std::string& entry : config.blacklist) {
        out += "blacklist=" + entry + "\n";
    }
    return out;
}

LibraryConfig parse(const QByteArray& raw) {
    LibraryConfig config;
    const QStringList lines =
        QString::fromUtf8(raw).split(QChar('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QChar('#'))) {
            continue;
        }
        const int equals = trimmed.indexOf(QChar('='));
        if (equals <= 0) {
            continue;  // not a tag=value record
        }
        const QString tag = trimmed.left(equals).trimmed();
        const QString value = trimmed.mid(equals + 1);
        if (tag == QLatin1String("folders")) {
            config.folders.push_back(value.toStdString());
        } else if (tag == QLatin1String("whitelist")) {
            config.whitelist.push_back(value.toStdString());
        } else if (tag == QLatin1String("blacklist")) {
            config.blacklist.push_back(value.toStdString());
        }
        // else: unknown tag from a future version — ignored, load keeps going.
    }
    return config;
}

}  // namespace

FileIndexStore::FileIndexStore(std::string filePath)
    : path_(filePath.empty() ? defaultStorePath() : std::move(filePath)) {}

void FileIndexStore::save(const LibraryConfig& config) {
    const QString target = QString::fromStdString(path_);
    QDir().mkpath(QFileInfo(target).absolutePath());

    QSaveFile file(target);  // temp file + atomic rename under the hood
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;  // port has no error reporting; keep previous state on disk
    }
    const std::string out = serialize(config);
    file.write(out.data(), static_cast<qint64>(out.size()));
    if (!file.commit()) {
        return;
    }
    savedInSession_ = true;
    sessionWrittenPaths().insert(path_);
}

LibraryConfig FileIndexStore::load() {
    QFile file(QString::fromStdString(path_));
    if (!file.open(QIODevice::ReadOnly)) {
        return LibraryConfig();  // missing or unreadable file: empty config
    }
    const LibraryConfig stored = parse(file.readAll());

    if (!savedInSession_ && sessionWrittenPaths().count(path_) != 0) {
        // Another instance in this process wrote the file, this store did
        // not: per the header this is a fresh session, not persisted state
        // to adopt.
        return LibraryConfig();
    }
    // Either this instance saved (re-read honestly from disk — overwrite
    // semantics must hold), or the file was written by another process
    // (real cross-process persistence across app restarts).
    return stored;
}

}  // namespace shanty
