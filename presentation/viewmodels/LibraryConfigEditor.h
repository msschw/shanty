// SH-1004 (UX-15): the shared library-config editing implementation — the
// responsibility extracted from LibraryViewModel so the SETTINGS side can
// run the SAME commands over the SAME store without duplicating a single
// rule (SRP / single decision point). Semantics and reason strings are the
// LibraryViewModel decisions V1–V4 + V6, locked by tests/library_vm (via
// the composing LibraryViewModel) and by tests/settings_vm (via the
// composing SettingsViewModel). Pure C++ with callbacks: no Qt, no
// exceptions.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../../core/domain/LibraryConfig.h"
#include "../../core/ports/ILibraryStore.h"

namespace shanty {

/// Owns a working copy of LibraryConfig (loaded through ILibraryStore) and
/// exposes the configuration editing (managed folders / whitelist /
/// blacklist). The listing/selection responsibilities stay with
/// LibraryViewModel (V5/V7); this class never scans.
///
/// Documented decisions (shared with LibraryViewModel — see its header for
/// the full V1..V8 record; the editor implements V1–V4 and V6):
///
///   V1 add/validation — an ADD that would introduce a bad entry is
///      rejected via onInvalid, NOT silently ignored; reason catalogue
///      (stable strings, asserted verbatim in BOTH composing suites):
///        "addFolder: empty path" · "addFolder: already managed"
///        "addWhitelistEntry: empty path" · "addBlacklistEntry: empty path"
///   V2 removals are idempotent — a removal that misses is a harmless
///      no-op: no callback, no save, no config change.
///   V3 whitelist/blacklist adds are set semantics — an already-present
///      entry is not duplicated and not an error (silent idempotent no-op).
///   V4 saving — IMMEDIATE save (exactly one store.save(config)) after
///      every EFFECTIVE mutation, before the method returns; no save on
///      rejections, idempotent no-ops or reload().
///   V6 config lifecycle — the constructor loads the working config through
///      store.load() exactly once (not a mutation); reload() re-loads
///      through the store, replacing the working config (no save, no
///      event). Cross-visibility (SH-1004): two editors over ONE store see
///      each other's writes through this reload path.
class LibraryConfigEditor {
public:
    // ---- observation hooks (Qt-free; composing view models reconnect) ----
    /// Fired for a rejected add (see V1).
    std::function<void(const char* reason)> onInvalid;
    /// Fired exactly once after each effective, persisted config mutation.
    std::function<void()> onConfigChanged;

    /// Loads the initial working config through the store (V6: exactly one
    /// store.load(), no save).
    explicit LibraryConfigEditor(ILibraryStore& store);

    /// The current working config (mutated by the methods below, loaded
    /// initially and by reload()).
    const LibraryConfig& config() const;
    /// Re-reads the working config from the store (V6).
    void reload();

    void addFolder(const std::string& path);
    void removeFolder(const std::string& path);
    void addWhitelistEntry(const std::string& path);
    void removeWhitelistEntry(const std::string& path);
    void addBlacklistEntry(const std::string& path);
    void removeBlacklistEntry(const std::string& path);

private:
    ILibraryStore& store_;

    LibraryConfig config_;

    /// Persists the working config (exactly one save) and notifies.
    void persist();
    bool contains(const std::vector<std::string>& list,
                  const std::string& path) const;
    /// Removes the first matching entry; returns true when the list changed.
    bool eraseFirst(std::vector<std::string>& list, const std::string& path);
};

}  // namespace shanty
