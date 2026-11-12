// SH-805 Phase B: the file-backed playback-session store (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "QueueStateStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <chrono>
#include <utility>

namespace shanty {

// ---------------------------------------------------------------------------
// Storage location (FileIndexStore/SH-204 idiom):
//
//   1. If the env var SHANTY_QUEUE_STORE is set, it is used verbatim as the
//      absolute target file path (documented test/development hook; unset it
//      on the device).
//
//   2. Otherwise the file lives at
//        <QStandardPaths::writableLocation(GenericConfigLocation)>
//        + "/shanty/queue.conf"
//      which maps into the Sailfish app sandbox on the device — the app's
//      config location, no absolute API paths hardcoded.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// File format ("shanty queue state", version 1, plain UTF-8 text):
//
//   # shanty queue state v1
//   state=paused
//   index=1
//   position=42
//   count=3
//   path0=/music/a.mp3
//   path1=/music/b.ogg
//   path2=/music/c.flac
//
// - One "tag=value" line per record: the first '=' separates tag and value,
//   so paths may contain '=' (they cannot contain newlines).
// - Fixed serialization order (state -> index -> position -> count ->
//   pathN), so equal states produce byte-identical files.
// - STRICT load: any missing/unknown-order-independent-but-required tag,
//   malformed integer, out-of-range index, or wrong path-line count makes
//   the whole file corrupt → a clean EMPTY restore (no partial adoption,
//   no crash, no exceptions). count=0 is a valid empty session.
// ---------------------------------------------------------------------------

namespace {

std::string defaultStorePath() {
    // Qt 5.6 compat (device target): qEnvironmentVariable() is Qt>=5.10, the
    // qEnvironmentVariableIsSet()/qgetenv() pair is available since 5.5.
    const QString overridePath = qEnvironmentVariableIsSet("SHANTY_QUEUE_STORE")
        ? QString::fromLocal8Bit(qgetenv("SHANTY_QUEUE_STORE"))
        : QString();
    if (!overridePath.isEmpty()) {
        return overridePath.toStdString();
    }
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    if (base.isEmpty()) {
        // No config location available: fall back to the per-user temp dir
        // rather than a relative path in the current working directory.
        base = QDir::tempPath();
    }
    return (base + "/shanty/queue.conf").toStdString();
}

std::string serialize(const QueueState& state) {
    std::string out;
    out += "# shanty queue state v1\n";
    out += "state=" + state.transportState + "\n";
    out += "index=" + std::to_string(state.index) + "\n";
    out += "position=" + std::to_string(state.positionSeconds) + "\n";
    out += "count=" + std::to_string(state.paths.size()) + "\n";
    for (std::size_t i = 0; i < state.paths.size(); ++i) {
        out += "path" + std::to_string(i) + "=" + state.paths[i] + "\n";
    }
    return out;
}

/// Strict v1 parser; an empty optional-like result is signalled by an empty
/// QueueState (a valid empty session also parses to an empty QueueState, so
/// both cases restore cleanly to "no session").
QueueState parse(const QByteArray& raw) {
    QueueState state;
    bool sawState = false, sawIndex = false, sawPosition = false;
    bool sawCount = false, headerSeen = false;
    std::size_t count = 0;
    std::size_t nextPath = 0;

    const QStringList lines = QString::fromUtf8(raw).split(QChar('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.startsWith(QChar('#'))) {
            if (trimmed.startsWith(QLatin1String("# shanty queue state"))) {
                headerSeen = true;
            }
            continue;  // comments are always ignored
        }
        const int equals = trimmed.indexOf(QChar('='));
        if (equals <= 0) {
            return QueueState();  // structural garbage → corrupt
        }
        const QString tag = trimmed.left(equals).trimmed();
        const QString value = trimmed.mid(equals + 1);
        bool numericOk = false;
        if (tag == QLatin1String("state")) {
            state.transportState = value.toStdString();
            sawState = true;
        } else if (tag == QLatin1String("index")) {
            const long long parsed = value.toLongLong(&numericOk);
            if (!numericOk || parsed < 0) {
                return QueueState();
            }
            state.index = static_cast<std::size_t>(parsed);
            sawIndex = true;
        } else if (tag == QLatin1String("position")) {
            const long long parsed = value.toLongLong(&numericOk);
            if (!numericOk || parsed < 0) {
                return QueueState();
            }
            state.positionSeconds = static_cast<long>(parsed);
            sawPosition = true;
        } else if (tag == QLatin1String("count")) {
            const long long parsed = value.toLongLong(&numericOk);
            if (!numericOk || parsed < 0 || parsed > 100000) {
                return QueueState();  // absurd count → corrupt
            }
            count = static_cast<std::size_t>(parsed);
            state.paths.reserve(count);
            sawCount = true;
        } else if (tag.startsWith(QLatin1String("path"))) {
            const QString suffix = tag.mid(4);
            bool indexOk = false;
            const long long pathIndex = suffix.toLongLong(&indexOk);
            if (!indexOk || pathIndex < 0 ||
                static_cast<std::size_t>(pathIndex) != nextPath) {
                return QueueState();  // out-of-order/duplicate path line
            }
            ++nextPath;
            state.paths.push_back(value.toStdString());
        } else {
            return QueueState();  // unknown tag in this strict format
        }
    }

    if (!headerSeen || !sawState || !sawIndex || !sawPosition || !sawCount ||
        nextPath != count) {
        return QueueState();  // truncated/malformed → clean empty restore
    }
    if (count > 0 && state.index >= count) {
        return QueueState();  // out-of-range index → corrupt
    }
    return state;
}

}  // namespace

QueueStateStore::QueueStateStore(std::string filePath, int debounceMs)
    : path_(filePath.empty() ? defaultStorePath() : std::move(filePath)),
      debounceMs_(debounceMs < 0 ? 0 : debounceMs) {}

QueueStateStore::~QueueStateStore() {
    // Documented dtor flush: a pending coalesced snapshot still lands on
    // disk when the application object goes away (clean app shutdown).
    flush();
}

void QueueStateStore::save(const QueueState& state) {
    pending_ = state;
    hasPending_ = true;

    if (debounceMs_ == 0) {
        writePending();  // write-through mode (tests, position ticks)
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (lastWrite_ == Clock::time_point() ||
        now - lastWrite_ >= std::chrono::milliseconds(debounceMs_)) {
        // First write ever, or the window since the last write has passed.
        writePending();
    }
    // else: inside the window — the pending snapshot coalesces; it lands on
    // disk with the next outside-window save, the explicit flush(), or the
    // destructor (the store always ends up with the LATEST snapshot).
}

QueueState QueueStateStore::load() const {
    QFile file(QString::fromStdString(path_));
    if (!file.open(QIODevice::ReadOnly)) {
        return QueueState();  // missing or unreadable file: no session
    }
    return parse(file.readAll());
}

void QueueStateStore::flush() {
    if (hasPending_) {
        writePending();
    }
}

void QueueStateStore::writePending() {
    const QString target = QString::fromStdString(path_);
    QDir().mkpath(QFileInfo(target).absolutePath());

    QSaveFile file(target);  // temp file + atomic rename under the hood
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;  // keep the previous state on disk
    }
    const std::string out = serialize(pending_);
    file.write(out.data(), static_cast<qint64>(out.size()));
    if (!file.commit()) {
        return;
    }
    hasPending_ = false;
    lastWrite_ = Clock::now();
}

}  // namespace shanty
