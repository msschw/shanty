// SH-603 Phase B: pure C++ playlist view model implementation. See
// PlaylistViewModel.h for the documented flow state machine, transition
// rules and callback ordering guarantees.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaylistViewModel.h"

namespace shanty {

PlaylistViewModel::PlaylistViewModel(PlaylistService& service)
    : service_(service) {
    // Re-emit the service's observation hooks as view-model events while
    // driving the flow state machine below (mutated → then changed is
    // preserved verbatim from the service).
    service_.onPlaylistChanged = [this]() {
        if (onEntriesChanged) onEntriesChanged();
    };
    service_.onImportFinished = [this](const ImportOutcome& outcome) {
        finishImport(outcome);
    };
    service_.onExportFinished = [this](const ExportOutcome& outcome) {
        finishExport(outcome);
    };
}

// ---- flow state machine ----------------------------------------------------
void PlaylistViewModel::beginImport() {
    if (flowState_ != PlaylistFlowState::Idle &&
        flowState_ != PlaylistFlowState::Done &&
        flowState_ != PlaylistFlowState::Failed) {
        notifyInvalid("beginImport: flow already open");
        return;
    }
    lastError_.clear();
    // SH-807 (UX-11): a dirty playlist would be destroyed by the import —
    // the flow first asks the user ("discard unsaved edits?"); confirm
    // proceeds to the file-picker flow, cancel keeps the edits.
    flowState_ = dirty_ ? PlaylistFlowState::AwaitingConfirm
                        : PlaylistFlowState::AwaitingFile;
    pendingOp_ = PendingOp::Import;
    notifyFlowChanged();
}

void PlaylistViewModel::beginExport() {
    if (flowState_ != PlaylistFlowState::Idle &&
        flowState_ != PlaylistFlowState::Done &&
        flowState_ != PlaylistFlowState::Failed) {
        notifyInvalid("beginExport: flow already open");
        return;
    }
    lastError_.clear();
    flowState_ = PlaylistFlowState::AwaitingFile;
    pendingOp_ = PendingOp::Export;
    notifyFlowChanged();
}

void PlaylistViewModel::confirmImport(const std::string& filePath) {
    if (flowState_ != PlaylistFlowState::AwaitingFile) {
        notifyInvalid("confirmImport: not awaiting file");
        return;
    }
    if (pendingOp_ == PendingOp::Export) {
        notifyInvalid("confirmImport: export pending");
        return;
    }
    lastError_.clear();
    flowState_ = PlaylistFlowState::Importing;
    notifyFlowChanged();
    // The service is synchronous: its callbacks land on finishImport and
    // move the state to Done|Failed before this call returns.
    service_.import(filePath);
}

void PlaylistViewModel::confirmExport(const std::string& filePath) {
    if (flowState_ != PlaylistFlowState::AwaitingFile) {
        notifyInvalid("confirmExport: not awaiting file");
        return;
    }
    if (pendingOp_ == PendingOp::Import) {
        notifyInvalid("confirmExport: import pending");
        return;
    }
    lastError_.clear();
    flowState_ = PlaylistFlowState::Exporting;
    notifyFlowChanged();
    // Same synchronous contract as confirmImport (see above).
    service_.exportPlaylist(filePath);
}

void PlaylistViewModel::cancelFlow() {
    switch (flowState_) {
        case PlaylistFlowState::AwaitingConfirm:
        case PlaylistFlowState::AwaitingFile:
        case PlaylistFlowState::Done:
        case PlaylistFlowState::Failed:
            lastError_.clear();  // a dismissed result drops the old error
            flowState_ = PlaylistFlowState::Idle;
            notifyFlowChanged();
            // SH-807: the edits are untouched by the cancel (AwaitingConfirm
            // cancel = "keep the edits" — dirty stays as it is).
            break;
        case PlaylistFlowState::Idle:
            notifyInvalid("cancel: no open flow");
            break;
        case PlaylistFlowState::Importing:
        case PlaylistFlowState::Exporting:
            notifyInvalid("cancel: flow in progress");
            break;
    }
}

void PlaylistViewModel::confirmDiscard() {
    if (flowState_ != PlaylistFlowState::AwaitingConfirm) {
        notifyInvalid("confirmDiscard: not awaiting confirm");
        return;
    }
    // The pending op is always Import here (AwaitingConfirm is reached
    // only through beginImport) — proceed to the file-picker flow. The
    // dirty state is NOT cleared yet: the discard takes effect only when
    // the import actually replaces the entries (a failed import keeps
    // the edits).
    flowState_ = PlaylistFlowState::AwaitingFile;
    notifyFlowChanged();
}

// ---- flow observation -------------------------------------------------------
PlaylistFlowState PlaylistViewModel::flowState() const { return flowState_; }

bool PlaylistViewModel::isDirty() const { return dirty_; }

const std::string& PlaylistViewModel::lastError() const { return lastError_; }

// ---- mutation delegation (SH-304 facade) -------------------------------------
// SH-807: each delegate snapshots the entries first and sets dirty_ only
// when the service actually changed them (domain-noop filter).
bool PlaylistViewModel::entriesChangedSince(
    const std::vector<PlaylistEntry>& snapshot) const {
    return service_.entries() != snapshot;
}

void PlaylistViewModel::add(const PlaylistEntry& entry) {
    const auto before = service_.entries();
    service_.add(entry);
    if (entriesChangedSince(before)) dirty_ = true;
}

void PlaylistViewModel::moveEntry(std::size_t from, std::size_t to) {
    const auto before = service_.entries();
    service_.moveEntry(from, to);
    if (entriesChangedSince(before)) dirty_ = true;
}

void PlaylistViewModel::removeAt(std::size_t index) {
    const auto before = service_.entries();
    service_.removeAt(index);
    if (entriesChangedSince(before)) dirty_ = true;
}

void PlaylistViewModel::clear() {
    const auto before = service_.entries();
    service_.clear();
    if (entriesChangedSince(before)) dirty_ = true;
}

void PlaylistViewModel::deduplicate() {
    const auto before = service_.entries();
    service_.deduplicate();
    if (entriesChangedSince(before)) dirty_ = true;  // only on real change
}

// ---- playlist observation (passthrough) ---------------------------------------
std::size_t PlaylistViewModel::entryCount() const { return service_.count(); }

const std::vector<PlaylistEntry>& PlaylistViewModel::entries() const {
    return service_.entries();
}

long long PlaylistViewModel::totalDurationSeconds() const {
    return service_.totalDurationSeconds();
}

// ---- SH-809 (UX-8): row context-menu command catalogue ------------------------

std::vector<PlaylistViewModel::RowCommand> PlaylistViewModel::rowCommands(
    std::size_t index) const {
    // Pure index validity: out of range → no menu. Valid → the SH-809
    // playlist-row catalogue (PlayFromHere through the row-tap play bridge,
    // Remove through removeAt); flow state and dirtiness do not filter it.
    if (index >= service_.count()) {
        return {};
    }
    return {RowCommand::PlayFromHere, RowCommand::Remove};
}

// ---- internal handlers ---------------------------------------------------------
void PlaylistViewModel::notifyFlowChanged() {
    if (onFlowChanged) onFlowChanged();
}

void PlaylistViewModel::notifyInvalid(const char* reason) {
    if (onInvalidCommand) onInvalidCommand(reason);
}

void PlaylistViewModel::finishImport(const ImportOutcome& outcome) {
    if (outcome.status == ImportOutcome::Status::Success) {
        lastError_.clear();
        dirty_ = false;  // SH-807: the imported entries are the new baseline
        flowState_ = PlaylistFlowState::Done;
    } else {
        // Codec errors surface with the codec's first error message,
        // unreadable files with the service's own message (see
        // PlaylistService.h decisions). The edits survive a failed import
        // (nothing was replaced) — the dirty state stays (SH-807).
        lastError_ = outcome.errorMessage;
        flowState_ = PlaylistFlowState::Failed;
    }
    notifyFlowChanged();
}

void PlaylistViewModel::finishExport(const ExportOutcome& outcome) {
    if (outcome.status == ExportOutcome::Status::Success) {
        lastError_.clear();
        dirty_ = false;  // SH-807: the work is saved
        flowState_ = PlaylistFlowState::Done;
    } else {
        lastError_ = outcome.errorMessage;
        flowState_ = PlaylistFlowState::Failed;
        // A failed export saved nothing — the edits stay dirty (SH-807).
    }
    notifyFlowChanged();
}

}  // namespace shanty
