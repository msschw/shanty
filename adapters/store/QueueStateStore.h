// SH-805 Phase A (stub) / Phase B: persists the playback session — the
// queue paths, selected index, position and transport state — so the next
// app start restores it paused (UX-4). See QueueStateStore.cpp for the file
// format and the location priority (SHANTY_QUEUE_STORE env override, else
// the Qt GenericConfigLocation, which maps into the Sailfish app sandbox);
// the path/atomic-write/session idioms follow FileIndexStore (SH-204).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace shanty {

/// One persisted playback-session snapshot (SH-805 / UX-4): the queue paths
/// in playlist order, the selected index, the position inside the selected
/// entry (seconds) and the transport-state string at save time
/// ("idle"/"playing"/"paused"/"stopped"; empty when the producer had nothing
/// meaningful). The APPLICATION always applies a restored snapshot paused
/// (restore-paused invariant, PlaybackController::restoreQueue) — the state
/// string is persisted for round-trip fidelity only.
struct QueueState {
    std::vector<std::string> paths;
    std::size_t index = 0;
    long positionSeconds = 0;
    std::string transportState;
};

inline bool operator==(const QueueState& a, const QueueState& b) {
    return a.paths == b.paths && a.index == b.index &&
           a.positionSeconds == b.positionSeconds &&
           a.transportState == b.transportState;
}

// SH-805 Phase B: file-backed queue-state store with a trailing-write
// debounce (queue mutation/track change/position tick can fire save() at
// high frequency; the store coalesces and always ends up with the LATEST
// snapshot — pending state is flushed on destruction).
class QueueStateStore {
public:
    /// Optional explicit target file; defaults to the deterministic sandbox
    /// location (SHANTY_QUEUE_STORE env override, else the config path).
    /// @param debounceMs coalescing window measured against the last actual
    /// disk write: a save() at least this many milliseconds after the last
    /// write goes to disk immediately; earlier saves are kept as the pending
    /// latest snapshot (0 = every save writes immediately; tests drive
    /// determinism with 0, a huge window and flush()).
    explicit QueueStateStore(std::string filePath = std::string(),
                             int debounceMs = 1000);
    /// Flushes a pending snapshot (never starts I/O otherwise).
    ~QueueStateStore();

    QueueStateStore(const QueueStateStore&) = delete;
    QueueStateStore& operator=(const QueueStateStore&) = delete;

    /// Records the snapshot; the disk write happens immediately (outside the
    /// debounce window) or is coalesced into the pending latest snapshot.
    void save(const QueueState& state);

    /// Reads the persisted snapshot; a missing/unreadable/corrupt/truncated
    /// file yields an EMPTY state (clean restore, no crash, no exceptions).
    QueueState load() const;

    /// Writes the pending snapshot immediately (documented shutdown hook).
    void flush();

private:
    void writePending();

    std::string path_;              // resolved target file for this store
    int debounceMs_;
    bool hasPending_ = false;
    QueueState pending_;
};

}  // namespace shanty
