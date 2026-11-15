// SH-1004 (UX-15): implementation of the shared library-config editing
// contract — moved verbatim from LibraryViewModel.cpp (whose behavioural
// record V1–V4/V6 it implements; see LibraryConfigEditor.h). Filtering is
// NOT here — LibraryService owns the composition (V5).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "LibraryConfigEditor.h"

#include <algorithm>

namespace shanty {

LibraryConfigEditor::LibraryConfigEditor(ILibraryStore& store)
    : store_(store) {
    // V6: the working config starts as the persisted one — exactly one
    // load, not a mutation (no save, no event).
    config_ = store_.load();
}

const LibraryConfig& LibraryConfigEditor::config() const { return config_; }

void LibraryConfigEditor::reload() {
    // V6: re-read through the store once. Not a mutation (V4): no save;
    // also not an event (V8) — observers that need the new state read
    // config() after the call. Cross-visibility path (SH-1004): this is
    // how the OTHER composing view model's writes become visible.
    config_ = store_.load();
}

void LibraryConfigEditor::addFolder(const std::string& path) {
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

void LibraryConfigEditor::removeFolder(const std::string& path) {
    // V2: removals are idempotent — a removal that misses nothing cannot
    // damage the persisted state, so it is a silent no-op (no rejection).
    if (eraseFirst(config_.folders, path)) {
        persist();
    }
}

void LibraryConfigEditor::addWhitelistEntry(const std::string& path) {
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

void LibraryConfigEditor::removeWhitelistEntry(const std::string& path) {
    if (eraseFirst(config_.whitelist, path)) {  // V2: idempotent no-op
        persist();
    }
}

void LibraryConfigEditor::addBlacklistEntry(const std::string& path) {
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

void LibraryConfigEditor::removeBlacklistEntry(const std::string& path) {
    if (eraseFirst(config_.blacklist, path)) {  // V2: idempotent no-op
        persist();
    }
}

void LibraryConfigEditor::persist() {
    // V4: immediate save — the mutation is durable before the method
    // returns; callers observe the event only after a successful save.
    store_.save(config_);
    if (onConfigChanged) {
        onConfigChanged();
    }
}

bool LibraryConfigEditor::contains(const std::vector<std::string>& list,
                                   const std::string& path) const {
    return std::find(list.begin(), list.end(), path) != list.end();
}

bool LibraryConfigEditor::eraseFirst(std::vector<std::string>& list,
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
