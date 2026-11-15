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
    : store_(store), library_(library), editor_(store) {
    // SH-1004: the editor performs the V6 initial load (exactly one
    // store.load() — unchanged contract); this class only wires the event
    // forwarding (V8 channels stay the composing view model's own hooks).
    editor_.onInvalid = [this](const char* reason) {
        if (onInvalid) {
            onInvalid(reason);
        }
    };
    editor_.onConfigChanged = [this]() {
        if (onConfigChanged) {
            onConfigChanged();
        }
    };
}

const LibraryConfig& LibraryViewModel::config() const {
    return editor_.config();
}

void LibraryViewModel::reload() {
    // V6: re-read through the store once; the tracked selection refers to a
    // listing that may no longer match the new folders → clear it (V6).
    editor_.reload();
    selected_.clear();
    // Not a mutation (V4): no save; also not an event (V8) — observers that
    // need the new state read config() after the call.
}

void LibraryViewModel::addFolder(const std::string& path) {
    editor_.addFolder(path);  // V1/V3/V4 semantics live in the editor
}

void LibraryViewModel::removeFolder(const std::string& path) {
    editor_.removeFolder(path);  // V2: idempotent no-op
}

void LibraryViewModel::addWhitelistEntry(const std::string& path) {
    editor_.addWhitelistEntry(path);  // V1/V3/V4 semantics live in the editor
}

void LibraryViewModel::removeWhitelistEntry(const std::string& path) {
    editor_.removeWhitelistEntry(path);  // V2: idempotent no-op
}

void LibraryViewModel::addBlacklistEntry(const std::string& path) {
    editor_.addBlacklistEntry(path);  // V1/V3/V4 semantics live in the editor
}

void LibraryViewModel::removeBlacklistEntry(const std::string& path) {
    editor_.removeBlacklistEntry(path);  // V2: idempotent no-op
}

const std::vector<std::string>& LibraryViewModel::tracks() const {
    return tracks_;  // the listing produced by the last scan() (V7)
}

std::vector<std::string> LibraryViewModel::scan() {
    // V5/V7: single passthrough — LibraryService owns the composition
    // (managed folders → supported extensions → blacklist beats whitelist).
    tracks_ = library_.scan(editor_.config());
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

// ---- SH-809 (UX-8): row context-menu command catalogue ------------------------

std::vector<LibraryViewModel::RowCommand> LibraryViewModel::rowCommands(
    std::size_t index) const {
    // Pure index validity: out of range → no menu. Valid → the SH-809
    // library-row catalogue (Play through the SH-802 bridge with just this
    // path, AddToQueue through the controller's enqueue); the selection
    // state does not filter it.
    if (index >= tracks_.size()) {
        return {};
    }
    return {RowCommand::Play, RowCommand::AddToQueue};
}

}  // namespace shanty
