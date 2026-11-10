// SH-602 Phase B: library view-model implementation. The behavioural
// contract (decisions V1..V8) lives in LibraryViewModel.h and is locked by
// tests/library_vm. Filtering is NOT re-implemented here — scan() is a
// passthrough to LibraryService (V5). Copyright 2026 Shanty contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryViewModel.h"

#include <algorithm>

namespace shanty {

LibraryViewModel::LibraryViewModel(ILibraryStore& store,
                                   LibraryService& library)
    : store_(store), library_(library) {
    // V6: the working config starts as the persisted one — exactly one load,
    // not a mutation (no save, no event).
    config_ = store_.load();
}

const LibraryConfig& LibraryViewModel::config() const { return config_; }

void LibraryViewModel::reload() {
    // V6: re-read through the store once; the tracked selection refers to a
    // listing that may no longer match the new folders → clear it (V6).
    config_ = store_.load();
    selected_.clear();
    // Not a mutation (V4): no save; also not an event (V8) — observers that
    // need the new state read config() after the call.
}

void LibraryViewModel::addFolder(const std::string& path) {
    // V1: adds that would introduce a bad entry are REJECTED, not ignored.
    if (path.empty()) {
        if (onInvalid) {
            onInvalid("addFolder: empty path");
        }
        return;
    }
    if (contains(config_.folders, path)) {
        if (onInvalid) {
            onInvalid("addFolder: already managed");
        }
        return;
    }
    config_.folders.push_back(path);  // append order — Phase B lists top-down
    persist();
}

void LibraryViewModel::removeFolder(const std::string& path) {
    // V2: removals are idempotent — a removal that misses nothing cannot
    // damage the persisted state, so it is a silent no-op (no rejection).
    if (eraseFirst(config_.folders, path)) {
        persist();
    }
}

void LibraryViewModel::addWhitelistEntry(const std::string& path) {
    // V1: empty paths are meaningless as prefixes for FilterRule.
    if (path.empty()) {
        if (onInvalid) {
            onInvalid("addWhitelistEntry: empty path");
        }
        return;
    }
    // V3: set semantics — an existing entry is not duplicated and not an
    // error (the filter lists are data; only the folder list is user-facing).
    if (contains(config_.whitelist, path)) {
        return;
    }
    config_.whitelist.push_back(path);
    persist();
}

void LibraryViewModel::removeWhitelistEntry(const std::string& path) {
    if (eraseFirst(config_.whitelist, path)) {  // V2: idempotent no-op
        persist();
    }
}

void LibraryViewModel::addBlacklistEntry(const std::string& path) {
    if (path.empty()) {  // V1 (mirror of the whitelist rule)
        if (onInvalid) {
            onInvalid("addBlacklistEntry: empty path");
        }
        return;
    }
    if (contains(config_.blacklist, path)) {  // V3: set semantics
        return;
    }
    config_.blacklist.push_back(path);
    persist();
}

void LibraryViewModel::removeBlacklistEntry(const std::string& path) {
    if (eraseFirst(config_.blacklist, path)) {  // V2: idempotent no-op
        persist();
    }
}

const std::vector<std::string>& LibraryViewModel::tracks() const {
    return tracks_;  // the listing produced by the last scan() (V7)
}

std::vector<std::string> LibraryViewModel::scan() {
    // V5/V7: single passthrough — LibraryService owns the composition
    // (managed folders → supported extensions → blacklist beats whitelist).
    tracks_ = library_.scan(config_);
    selected_.clear();  // indices belong to one listing only (V7)
    return tracks_;
}

void LibraryViewModel::selectAll() {
    // V7: select the whole CURRENT listing; an empty listing selects nothing
    // (that is the correct state, not a rejection).
    selected_.clear();
    for (std::size_t index = 0; index < tracks_.size(); ++index) {
        selected_.push_back(index);
    }
}

void LibraryViewModel::clearSelection() { selected_.clear(); }

void LibraryViewModel::setSelected(std::size_t index, bool selected) {
    // V1: an index outside the current listing is a rejected no-op.
    if (index >= tracks_.size()) {
        if (onInvalid) {
            onInvalid("setSelected: index out of range");
        }
        return;
    }
    const std::vector<std::size_t>::iterator it =
        std::find(selected_.begin(), selected_.end(), index);
    if (selected && it == selected_.end()) {
        selected_.push_back(index);
    } else if (!selected && it != selected_.end()) {
        selected_.erase(it);
    }
    // Ordered view: selection() sorts ascending on read (V7).
}

std::vector<std::size_t> LibraryViewModel::selection() const {
    std::vector<std::size_t> sorted = selected_;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

std::vector<std::string> LibraryViewModel::selectionPaths() const {
    std::vector<std::string> paths;
    for (const std::size_t index : selection()) {
        paths.push_back(tracks_[index]);
    }
    return paths;  // ascending listing order — queue-ready input for SH-604
}

void LibraryViewModel::persist() {
    // V4: immediate save — the mutation is durable before the method
    // returns; callers observe the event only after a successful save.
    store_.save(config_);
    if (onConfigChanged) {
        onConfigChanged();
    }
}

bool LibraryViewModel::contains(const std::vector<std::string>& list,
                                const std::string& path) const {
    return std::find(list.begin(), list.end(), path) != list.end();
}

bool LibraryViewModel::eraseFirst(std::vector<std::string>& list,
                                  const std::string& path) {
    const std::vector<std::string>::iterator it =
        std::find(list.begin(), list.end(), path);
    if (it == list.end()) {
        return false;
    }
    list.erase(it);
    return true;
}

}  // namespace shanty
