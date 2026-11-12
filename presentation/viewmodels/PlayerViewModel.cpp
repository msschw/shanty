// SH-604 Phase B: PlayerViewModel implementation. The documented decisions
// (observation model, progress mapping, seek clamp/rejection + the SH-402
// engine-hop seam, queue-edge flags, fatal-error gate and recovery) live in
// PlayerViewModel.h; each is encoded in tests/player_vm/tst_player_vm.cpp.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/viewmodels/PlayerViewModel.h"

namespace shanty {

PlayerViewModel::PlayerViewModel(PlaybackController& controller,
                                 PlaybackQueue& queue)
    : controller_(controller), queue_(queue) {}

// ---- button actions: thin forwards (the controller owns every decision) ----

void PlayerViewModel::togglePlayPause() { controller_.togglePlayPause(); }

void PlayerViewModel::next() { controller_.next(); }

void PlayerViewModel::previous() { controller_.previous(); }

void PlayerViewModel::retryPlayback() {
    // Documented recovery from the fatal-error gate: the SH-404 fresh
    // attempt — clears the recorded error and replays the selected entry.
    controller_.play();
}

void PlayerViewModel::playFromPaths(const std::vector<std::string>& paths) {
    if (paths.empty()) {
        // Documented rejection (SH-802 / UX-2): nothing to queue — the
        // queue and the transport state stay untouched.
        if (onInvalidCommand) {
            onInvalidCommand("play: playlist is empty");
        }
        return;
    }
    // Playlist order → queue order, selection → index 0, no autoplay
    // (PlaybackController::loadQueue contract), then the explicit start.
    controller_.loadQueue(paths);
    controller_.play();
    // SH-803 (UX-7): the queue-building action confirms (sparse rule —
    // transport changes never do). Verbatim catalogue wording, count =
    // queue size after the replace; fired BEFORE onPlayStarted so the
    // toast exists when the navigation to the Player starts.
    if (onConfirmed) {
        const std::size_t count = paths.size();
        const std::string message =
            count == 1 ? "queue: built from playlist (1 entry)"
                       : "queue: built from playlist (" +
                             std::to_string(count) + " entries)";
        onConfirmed(message.c_str());
    }
    if (onPlayStarted) {
        onPlayStarted();  // success only — never on the rejection path
    }
}

void PlayerViewModel::seekTo(long targetSeconds) {
    // Seek rules (PlayerViewModel.h): clamp into [0, durationSeconds-of-
    // snapshot]; rejected (no clamp anchor) without a known duration. The
    // clamped command is surfaced through onSeekRequested — the one-line
    // hop the application layer wires to the pending SH-402 engine seek.
    const long duration = durationSeconds_;
    if (duration <= 0) {
        if (onSeekRejected) {
            onSeekRejected();
        }
        return;
    }
    long clamped = targetSeconds;
    if (clamped < 0) {
        clamped = 0;
    } else if (clamped > duration) {
        clamped = duration;
    }
    if (onSeekRequested) {
        onSeekRequested(clamped);
    }
}

// ---- the single re-read entry point ----------------------------------------

void PlayerViewModel::refresh() {
    // Snapshots everything in one pass: engine position/duration via the
    // controller passthrough, queue observables, and the last error.
    state_ = controller_.state();
    currentEntry_ = controller_.currentEntry();
    currentIndex_ = controller_.currentIndex();
    queueCount_ = queue_.count();
    positionSeconds_ = controller_.positionSeconds();
    durationSeconds_ = controller_.durationSeconds();
    // SH-808: the queue-order entries snapshot (same staleness contract;
    // a copy — later queue mutations never mutate the published list).
    queueEntries_ = queue_.entries();

    hasError_ = controller_.hasError();
    if (controller_.hasError()) {
        const PlaybackError& error = controller_.lastError();
        errorCode_ = error.code;
        errorMessage_ = error.message;
        errorPath_ = error.path;
    } else {
        // Neutral values; the UI panel reads hasError() first anyway.
        errorCode_ = PlaybackError::Code::EngineFailure;
        errorMessage_.clear();
        errorPath_.clear();
    }

    if (onUpdated) {
        onUpdated();
    }
}

// ---- snapshot getters -------------------------------------------------------

const std::vector<std::string>& PlayerViewModel::queueEntries() const {
    // SH-808: the copy taken by the last refresh() — a plain member, so a
    // later queue mutation (replace/remove) never mutates the published
    // list; the next refresh() re-reads it. Read-only by decision.
    return queueEntries_;
}

PlaybackController::State PlayerViewModel::state() const { return state_; }

std::string PlayerViewModel::currentEntry() const { return currentEntry_; }

std::size_t PlayerViewModel::currentIndex() const { return currentIndex_; }

std::size_t PlayerViewModel::queueCount() const { return queueCount_; }

long PlayerViewModel::positionSeconds() const { return positionSeconds_; }

long PlayerViewModel::durationSeconds() const { return durationSeconds_; }

// ---- derived (pure functions over the snapshot) ------------------------------

double PlayerViewModel::progressFraction() const {
    // Documented rules: duration ≤ 0 (incl. kUnknownDuration = -1) → 0.0;
    // position < 0 → 0.0; position ≥ duration → 1.0; else the quotient.
    const long duration = durationSeconds_;
    if (duration <= 0) {
        return 0.0;
    }
    const long position = positionSeconds_;
    if (position < 0) {
        return 0.0;
    }
    if (position >= duration) {
        return 1.0;  // position beyond duration clamps; exactly at → 1.0
    }
    return static_cast<double>(position) / static_cast<double>(duration);
}

bool PlayerViewModel::canGoNext() const {
    // SH-401 stop-at-end, no wrap: false at the last entry and on an empty
    // queue; computed against the REMAINING entries.
    return queueCount_ > 0 && currentIndex_ + 1 < queueCount_;
}

bool PlayerViewModel::canGoPrevious() const {
    // No wrap: false at index 0 and on an empty queue.
    return queueCount_ > 0 && currentIndex_ > 0;
}

bool PlayerViewModel::transportEnabled() const {
    // Documented gate: disabled exactly in the FATAL stop state (SH-403
    // matrix — DecodeFailure/EngineFailure → stop, error recorded). A
    // SkippedUnsupported stop at the queue end matches on the CODE, not the
    // stopped state, and never disables. End-of-queue terminal stop records
    // no error and does not disable either.
    if (!hasError_ || state_ != PlaybackController::State::Stopped) {
        return true;
    }
    return errorCode_ != PlaybackError::Code::DecodeFailure &&
           errorCode_ != PlaybackError::Code::EngineFailure;
}

// ---- error observation -------------------------------------------------------

bool PlayerViewModel::hasError() const { return hasError_; }

PlaybackError::Code PlayerViewModel::lastErrorCode() const {
    return errorCode_;
}

const std::string& PlayerViewModel::lastErrorMessage() const {
    return errorMessage_;
}

const std::string& PlayerViewModel::lastErrorPath() const {
    return errorPath_;
}

}  // namespace shanty
