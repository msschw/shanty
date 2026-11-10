// SH-602 Phase A: library view-model — the application/presentation facade
// for the library view (Epic SH-E6). Pure C++ with std::function callbacks
// (same pattern as core/playback/PlaybackController, SH-404): no Qt, no
// exceptions — callbacks instead of exceptions for flow control. The Silica
// UI (Phase B, FolderPickerPage / folder list / multi-select) and the queue
// wiring to PlaybackController (SH-604) sit on top of this class.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "../../core/domain/LibraryConfig.h"
#include "../../core/ports/ILibraryStore.h"
#include "../../core/services/LibraryService.h"

namespace shanty {

/// Owns a working copy of LibraryConfig (loaded through ILibraryStore) and
/// exposes the filtered library listing (a scan() passthrough — LibraryService
/// owns filtering, this class never re-implements it) plus a small selection
/// API for queue building.
///
/// Documented decisions (each under test in tests/library_vm):
///
///   V1 add/validation — any ADD that would introduce a bad entry is
///      rejected, NOT silently ignored: for *all three* configuration lists,
///      an empty path or (for folders only) an already-managed path fires the
///      `onInvalid` callback and leaves the config and persistence untouched.
///      Reason catalogue (stable strings for the QML layer, asserted in the
///      suite):
///        "addFolder: empty path" · "addFolder: already managed"
///        "addWhitelistEntry: empty path" · "addBlacklistEntry: empty path"
///        "setSelected: index out of range"
///   V2 removals are idempotent — removing a non-present entry (folder,
///      whitelist or blacklist) is a harmless no-op: no callback, no save,
///      no config change. (In contrast to adds, a removal that misses cannot
///      damage the persisted state.)
///   V3 whitelist/blacklist adds are set semantics — an already-present
///      whitelist/blacklist entry is not duplicated and not an error (silent
///      idempotent no-op, unlike folders whose duplicates DO get user
///      feedback: the folder list is user-facing, the filter lists are data).
///   V4 saving — IMMEDIATE save after every *effective* triggering mutation
///      (add/remove that actually changed the config): exactly one
///      store.save(config) per mutation, before the method returns (crash
///      safety, no dirty-flag buffer to lose). No save on rejections,
///      idempotent no-ops, reload(), scan() or selection changes.
///   V5 filtering is delegated — blacklist-beats-whitelist and the whole
///      composition rule stay inside LibraryService (SH-202). The view-model
///      only edits the config and requests a scan; scan() is a passthrough.
///   V6 config lifecycle — the constructor loads the working config
///      through store.load() exactly once (NOT a mutation: no save, no
///      event); reload() re-loads through the store, replacing the working
///      config, and clears the selection (indices refer to a listing that
///      may no longer match the new folders).
///   V7 listing/selection coupling — scan() runs LibraryService::scan()
///      exactly once with the CURRENT working config, stores the result as
///      the listing and resets the selection. selection() returns the
///      ascending indices of the current listing; selectionPaths() resolves
///      them to paths in ascending order (queue-ready order, consumed by
///      SH-604).
///   V8 events — one `onInvalid` channel (shared rejection channel, mirroring
///      PlaybackController::onInvalidCommand) and one `onConfigChanged`
///      fired once after each effective persisted mutation (after the save).
///      Selection changes carry no event (Phase B property bindings observe
///      them synchronously after the call).
class LibraryViewModel {
public:
    // ---- observation hooks (Qt-free; the QObject wrapper reconnects them) --
    /// Fired for a rejected add / invalid selection index (see V1).
    std::function<void(const char* reason)> onInvalid;
    /// Fired exactly once after each effective, persisted config mutation.
    std::function<void()> onConfigChanged;

    /// Loads the initial working config through the store (V6: exactly one
    /// store.load(), no save).
    LibraryViewModel(ILibraryStore& store, LibraryService& library);

    // ---- configuration (folders / whitelist / blacklist) -------------------
    /// The current working config (mutated by the methods below, loaded
    /// initially and by reload()).
    const LibraryConfig& config() const;
    /// Re-reads the working config from the store (V6) and clears the
    /// selection.
    void reload();

    void addFolder(const std::string& path);
    void removeFolder(const std::string& path);
    void addWhitelistEntry(const std::string& path);
    void removeWhitelistEntry(const std::string& path);
    void addBlacklistEntry(const std::string& path);
    void removeBlacklistEntry(const std::string& path);

    // ---- listing (SH-202 passthrough, V5/V7) --------------------------------
    /// Last listing returned by scan(). Empty until the first scan().
    const std::vector<std::string>& tracks() const;
    /// Runs one LibraryService::scan(config()) with the CURRENT working
    /// config, stores it as the listing and resets the selection (V7).
    std::vector<std::string> scan();

    // ---- selection (queue-ready data for SH-604) ----------------------------
    void selectAll();
    void clearSelection();
    /// Flips membership of `index` in the selection; an out-of-range index is
    /// a rejected no-op via onInvalid (V1 catalogue).
    void setSelected(std::size_t index, bool selected);
    /// Selected indices of the current listing, ascending.
    std::vector<std::size_t> selection() const;
    /// The selected tracks' paths, ascending by listing order.
    std::vector<std::string> selectionPaths() const;

private:
    ILibraryStore& store_;
    LibraryService& library_;

    LibraryConfig config_;
    std::vector<std::string> tracks_;
    std::vector<std::size_t> selected_;

    /// Persists the working config (exactly one save) and notifies.
    void persist();
    bool contains(const std::vector<std::string>& list,
                  const std::string& path) const;
    /// Removes the first matching entry; returns true when the list changed.
    bool eraseFirst(std::vector<std::string>& list, const std::string& path);
};

}  // namespace shanty
