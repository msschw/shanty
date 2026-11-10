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
    flowState_ = PlaylistFlowState::AwaitingFile;
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
        case PlaylistFlowState::AwaitingFile:
        case PlaylistFlowState::Done:
        case PlaylistFlowState::Failed:
            lastError_.clear();  // a dismissed result drops the old error
            flowState_ = PlaylistFlowState::Idle;
            notifyFlowChanged();
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

// ---- flow observation -------------------------------------------------------
PlaylistFlowState PlaylistViewModel::flowState() const { return flowState_; }

const std::string& PlaylistViewModel::lastError() const { return lastError_; }

// ---- mutation delegation (SH-304 facade) -------------------------------------
void PlaylistViewModel::add(const PlaylistEntry& entry) { service_.add(entry); }

void PlaylistViewModel::moveEntry(std::size_t from, std::size_t to) {
    service_.moveEntry(from, to);
}

void PlaylistViewModel::removeAt(std::size_t index) {
    service_.removeAt(index);
}

void PlaylistViewModel::clear() { service_.clear(); }

void PlaylistViewModel::deduplicate() { service_.deduplicate(); }

// ---- playlist observation (passthrough) ---------------------------------------
std::size_t PlaylistViewModel::entryCount() const { return service_.count(); }

const std::vector<PlaylistEntry>& PlaylistViewModel::entries() const {
    return service_.entries();
}

long long PlaylistViewModel::totalDurationSeconds() const {
    return service_.totalDurationSeconds();
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
        flowState_ = PlaylistFlowState::Done;
    } else {
        // Codec errors surface with the codec's first error message,
        // unreadable files with the service's own message (see
        // PlaylistService.h decisions).
        lastError_ = outcome.errorMessage;
        flowState_ = PlaylistFlowState::Failed;
    }
    notifyFlowChanged();
}

void PlaylistViewModel::finishExport(const ExportOutcome& outcome) {
    if (outcome.status == ExportOutcome::Status::Success) {
        lastError_.clear();
        flowState_ = PlaylistFlowState::Done;
    } else {
        lastError_ = outcome.errorMessage;
        flowState_ = PlaylistFlowState::Failed;
    }
    notifyFlowChanged();
}

}  // namespace shanty
