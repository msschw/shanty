// SH-603: pure C++ playlist view model (PlaybackController repository
// pattern: facade over an application service, observation via callbacks,
// no Qt, no exceptions). It exposes the SH-304 playlist mutations through
// PlaylistService (ONE facade) and owns the import/export dialog FLOW as a
// state machine.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Documented decisions (SH-603; encoded in tst_playlist_vm):
//   - Flow state machine:
//       Idle --beginImport()/beginExport()--> AwaitingFile
//         (valid only from Idle/Done/Failed; from an open flow —
//         AwaitingFile/Importing/Exporting — the call is an invalid no-op
//         via onInvalidCommand, state preserved).
//       AwaitingFile --confirmImport(path)--> Importing and
//       AwaitingFile --confirmExport(path)--> Exporting.
//         Importing/Exporting are short synchronous hops: the service
//         runs inside the confirm call and its callbacks move the state
//         to Done (success) or Failed (error) BEFORE the confirm call
//         returns. confirm only at AwaitingFile is valid; elsewhere ->
//         invalid no-op with the state preserved. A pending operation
//         must not be switched: a confirmImport while an export flow is
//         pending (and vice versa) is rejected.
//         The view model owns the pending operation internally; in the
//         Sailfish UI later, beginImport/beginExport open the Silica file
//         dialog (target scope: import picks a source file, export picks
//         the export target — the export overwrites the chosen file
//         unconditionally, documented in PlaylistService.h).
//       Done/Failed --cancelFlow()--> Idle          (result dismissed)
//       AwaitingFile --cancelFlow()--> Idle         (dialog dismissed)
//       Importing/Exporting --cancelFlow()--> invalid no-op (synchronous
//         hop; unreachable from user code at rest).
//       Idle --cancelFlow()--> invalid no-op ("cancel: no open flow").
//   - onFlowChanged fires after every ACTUAL transition (Idle →
//     AwaitingFile → Importing → Done is therefore three observed
//     transitions; redundant commands fire nothing).
//   - Failure surfacing: lastError() carries the service error message
//     when a flow ended Failed ("" otherwise). It is cleared on every
//     beginImport/beginExport and cancelFlow (a fresh attempt or a
//     dismissed result drops the old error, PlaybackController play()
//     rule) and set when the service callback reports a failure.
//   - Mutation delegation (SH-304 via PlaylistService): every mutation
//     delegate (add/moveEntry/removeAt/clear/deduplicate) forwards to the
//     service exactly once and re-emits onEntriesChanged once per service
//     change notification — preserving the service's ordering guarantee
//     "mutated → then changed" (the callback observes the already-updated
//     state, also for domain no-ops).
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "services/PlaylistService.h"

namespace shanty {

/// Import/export dialog flow of the playlist page.
enum class PlaylistFlowState {
    Idle,             // no flow open (initial state / after cancelFlow)
    AwaitingConfirm,  // SH-807 (UX-11): "discard unsaved edits?" prompt up
                      // (beginImport while dirty; confirm → AwaitingFile,
                      // cancel → Idle with the edits kept)
    AwaitingFile,     // file dialog open (import or export target pending)
    Importing,        // playlist file being imported (synchronous hop)
    Exporting,        // playlist being exported (synchronous hop)
    Done,             // last operation finished successfully
    Failed,           // last operation failed (lastError() carries the reason)
};

class PlaylistViewModel {
public:
    explicit PlaylistViewModel(PlaylistService& service);

    // ---- observation hooks -------------------------------------------------
    /// Fired after every actual flow-state transition.
    std::function<void()> onFlowChanged;
    /// Fired once per onPlaylistChanged of the service (ordering guarantee:
    /// the entries are already updated at callback time).
    std::function<void()> onEntriesChanged;
    /// Fired for a command rejected as a documented no-op. Reason catalogue
    /// (kept stable for the QML layer):
    ///   "beginImport: flow already open"  · "beginExport: flow already open"
    ///   "confirmImport: not awaiting file" · "confirmExport: not awaiting file"
    ///   "confirmImport: export pending"    · "confirmExport: import pending"
    ///   "cancel: no open flow"             · "cancel: flow in progress"
    std::function<void(const char* reason)> onInvalidCommand;

    // ---- import/export dialog flow (see header decisions) ------------------
    /// Requests the import file dialog: Idle/Done/Failed → AwaitingFile.
    void beginImport();
    /// Requests the export file dialog: Idle/Done/Failed → AwaitingFile.
    void beginExport();
    /// Imports the chosen playlist file: AwaitingFile(import pending) →
    /// Importing → Done|Failed (before this call returns).
    void confirmImport(const std::string& filePath);
    /// Exports the playlist to the chosen file: AwaitingFile(export
    /// pending) → Exporting → Done|Failed (before this call returns).
    void confirmExport(const std::string& filePath);
    /// Dismisses an open dialog or a finished result → Idle.
    void cancelFlow();

    /// SH-807 (UX-11): confirms the "discard unsaved edits?" prompt
    /// (AwaitingConfirm) → the pending import proceeds to the file-picker
    /// flow (AwaitingFile). Valid ONLY from AwaitingConfirm.
    void confirmDiscard();

    // ---- flow observation ---------------------------------------------------
    PlaylistFlowState flowState() const;
    /// SH-807 (UX-11): true when entries were edited since the last
    /// SUCCESSFUL import/export (edits the next import would destroy).
    bool isDirty() const;
    /// Service error message of the last failed flow run; "" when none
    /// (cleared by the next beginImport/beginExport and by cancelFlow —
    /// a dismissed result drops the old error).
    const std::string& lastError() const;

    // ---- playlist mutation delegation (SH-304 facade, ORDERING "mutated →
    // changed" preserved; see header decisions) --------------------------------
    void add(const PlaylistEntry& entry);
    void moveEntry(std::size_t from, std::size_t to);
    void removeAt(std::size_t index);
    void clear();
    void deduplicate();

    // ---- playlist observation (passthrough) ---------------------------------
    std::size_t entryCount() const;
    const std::vector<PlaylistEntry>& entries() const;
    long long totalDurationSeconds() const;

private:
    enum class PendingOp { Import, Export };

    PlaylistService& service_;
    PlaylistFlowState flowState_ = PlaylistFlowState::Idle;
    PendingOp pendingOp_ = PendingOp::Import;
    std::string lastError_;

    void notifyFlowChanged();
    void notifyInvalid(const char* reason);
    void finishImport(const ImportOutcome& outcome);
    void finishExport(const ExportOutcome& outcome);
};

}  // namespace shanty
