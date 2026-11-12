// SH-604 Phase A/B: PlayerViewModel — the pure C++ view-model behind the
// Silica now-playing page (Phase B, epic-owned QML layer later). Repo pattern
// like core/playback/PlaybackController (SH-404): callbacks, not exceptions;
// no Qt; the QML wrapper must hold a reference to this class only.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Documented decisions (each encoded in tests/player_vm/tst_player_vm.cpp):
//
// Observation model — WHEN the view model re-reads the controller:
//   refresh() is the SINGLE re-read entry point. The UI's ~1 Hz position
//   timer calls refresh(), and the application layer additionally calls it
//   whenever controller.onStateChanged() fires. Every getter returns the
//   snapshot taken by the last refresh(); between refreshes the snapshot is
//   intentionally stale (asserted: engine values changed → getter unchanged
//   until the next refresh()). The view-model is NON-INTRUSIVE: it never
//   assigns the controller's onStateChanged/onInvalidCommand hooks — those
//   are single-slot std::functions owned by the application layer (the
//   SH-404 suite asserts on them).
//
// Dependencies:
//   Built against the same PlaybackController/PlaybackQueue composite the
//   application layer owns. The queue reference is needed because the
//   controller exposes no count accessor and the queue-edge decisions below
//   need "remaining entries". No other state is kept.
//
// Progress mapping (pure):
//   progressFraction() = positionSeconds() / durationSeconds() with rules
//   (each under test):
//     - duration ≤ 0 (including Track::kUnknownDuration = -1 and 0) → 0.0
//       (no known span → no meaningful fraction)
//     - position < 0 → 0.0 (defensive clamp; engines should not report
//       negatives, but the mapping stays pure and total)
//     - position ≥ duration → 1.0 (clamped, never > 1.0 for overshoot)
//     - otherwise the exact double quotient.
//   The engine values are observed through the controller's engine
//   passthrough at refresh() time; this keeps ONE dependency direction.
//
// Seek (documented deviation):
//   The SH-402 IAudioEngine port — a READ-ONLY green foundation for this
//   ticket — defines no seek primitive yet, and the SH-404 facade does not
//   add one. The view-model therefore owns clamp + rejection and surfaces
//   the clamped command through onSeekRequested(clampedSeconds), which the
//   application layer wires to the pending engine seek (one line once the
//   engine has the primitive). The callback argument IS the asserted
//   passthrough argument. Rules (each under test):
//     - clamped into [0, durationSeconds-of-snapshot]: negative → 0,
//       beyond duration → duration;
//     - duration unknown (≤ 0 in the snapshot, incl. unknown) → rejected:
//       onSeekRejected fires, onSeekRequested does NOT (there is no clamp
//       anchor);
//     - the duration consulted is the LAST REFRESHED snapshot, consistent
//       with the progress bar the seek bar drives.
//
// Derived disabled states at queue edges (SH-404/SH-401 stop-at-end, no wrap):
//   canGoPrevious() = queue non-empty AND currentIndex() > 0
//     (false at index 0 — no wrap-around, nothing before the first entry);
//   canGoNext() = queue non-empty AND currentIndex() + 1 < queueCount()
//     (computed against remaining entries: false at the last entry —
//     stop-at-end, no wrap);
//   both false on an empty queue. Buttons still forward presses to the
//   controller (the view-model never second-guesses it; invalid presses are
//   documented controller no-ops), but the UI disables them from these flags.
//
// Queue-replace play command (SH-802 / UX-2, decision recorded here):
//   playFromPaths(paths) is the ONE command behind "play a playlist": the
//   single decision point that (a) rejects an EMPTY input with the stable
//   catalogue string "play: playlist is empty" through onInvalidCommand —
//   queue and playback state untouched — and (b) otherwise replaces the
//   queue with the given paths in the given order (controller loadQueue:
//   selection → index 0) and starts playing (controller play()). The given
//   vector is COPIED into the queue (queue-snapshot semantics: later
//   mutations of the caller's list never mutate the running queue). The
//   command deliberately carries no navigation — the Playlist page decides
//   to show the Player from the onPlayStarted hook (the facade's mirror).
//   onPlayStarted fires ONLY on success (never on the rejection path).
//
// Confirmation channel (SH-803 / UX-7, decision recorded here):
//   onConfirmed(message) is the SPARSE positive-feedback channel — it
//   mirrors the rejection channel (stable catalogue string → shell toast)
//   but only queue-building/restoring actions ever fire it; per-track
//   transport changes (play/pause/next/previous/jumpTo on an existing
//   queue) deliberately NEVER confirm (the Player page and cover already
//   surface those). Producers so far: the SH-802 queue-replace play, whose
//   success confirms with the verbatim catalogue string
//     "queue: built from playlist (N entries)"
//   (singular form "queue: built from playlist (1 entry)" for N == 1;
//   the count is the queue size after the replace). Reserved wording for
//   SH-805 (queue persistence): "queue: restored" — implemented with that
//   ticket, same channel and sparse rule. The Library "Play selection"
//   producer adopts the same channel/wording when its flow is next
//   touched (SH-602 follow-up, not this ticket).
//
// Fatal-error transport gate (SH-403 decision matrix observed):
//   transportEnabled() == false exactly in the FATAL stop state:
//   hasError() && state() == Stopped && lastErrorCode() is DecodeFailure or
//   EngineFailure. (A real SkippedUnsupported skip may end Stopped at the
//   queue end — the gate matches on the CODE, not on the stopped state, so
//   a skipped-unsupported stop must NOT disable transport; asserted.)
//   While the gate is closed, togglePlayPause/next/previous/seek are
//   reported disabled; recovery is the explicit retryPlayback() — which
//   calls playbackController play(), the SH-404 documented fresh attempt
//   (clears the recorded error, replays the selected entry) and
//   re-enables transport after the next refresh().
//
// Error observation for the UI panel:
//   hasError()/lastErrorCode()/lastErrorMessage()/lastErrorPath() mirror
//   the controller's lastError() as of the last refresh() (code + message +
//   failing path), cleared together with the controller's error.
#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "playback/PlaybackController.h"
#include "playback/PlaybackQueue.h"
#include "ports/IAudioEngine.h"

namespace shanty {

class PlayerViewModel {
public:
    /// controller and queue must be the app layer's composite (the
    /// controller was built against this queue).
    PlayerViewModel(PlaybackController& controller, PlaybackQueue& queue);

    // ---- observation hooks (pure C++ callbacks, no Qt) ---------------------
    /// Clamped seek command; the application layer forwards it to the engine
    /// primitive once SH-402 defines it (see seek() deviation note above).
    std::function<void(long clampedSeconds)> onSeekRequested;
    /// Fired instead of onSeekRequested when the duration is unknown (≤ 0).
    std::function<void()> onSeekRejected;
    /// Fired for a view-model command rejected as a documented no-op
    /// (SH-802: the queue-replace play command's catalogue strings; mirrored
    /// by PlayerFacade::invalidCommand like the controller's own rejections).
    std::function<void(const char* reason)> onInvalidCommand;
    /// Fired exactly once on queue-replace play success (SH-802; never on
    /// the rejection path — the Playlist page navigates from its mirror).
    std::function<void()> onPlayStarted;
    /// SH-803 / UX-7: the sparse positive-feedback channel (shell toast).
    /// Only queue-building/restoring actions fire it — never per-track
    /// transport changes; message is the stable catalogue string (see the
    /// header decision notes for the locked wording).
    std::function<void(const char* message)> onConfirmed;
    /// Fired exactly once at the end of every refresh().
    std::function<void()> onUpdated;

    // ---- button actions (thin forwards to the controller) ------------------
    /// Explicit PR requirement: the play/pause button maps here, state
    /// semantics are the controller's (Idle/Stopped→play, Playing→pause,
    /// Paused→resume).
    void togglePlayPause();
    void next();
    void previous();
    /// Clamped/rejected seek command (rules and deviation documented above).
    void seekTo(long targetSeconds);
    /// Recover after a fatal error: documented fresh-attempt play().
    void retryPlayback();
    /// SH-802 (UX-2): the queue-replace play command — replaces the queue
    /// with the given paths (playlist order, index 0) and starts playing;
    /// empty input is a documented rejection ("play: playlist is empty",
    /// state untouched). See the header decision notes.
    void playFromPaths(const std::vector<std::string>& paths);

    // ---- the single re-read entry point (UI timer + state changes) ---------
    void refresh();

    // ---- snapshot getters (valid as of the last refresh()) -----------------
    PlaybackController::State state() const;
    /// Empty string when nothing is selected. Mirror of the controller.
    std::string currentEntry() const;
    std::size_t currentIndex() const;
    std::size_t queueCount() const;
    long positionSeconds() const;
    long durationSeconds() const;

    // ---- derived (pure functions over the snapshot) ------------------------
    double progressFraction() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    /// False exactly in the fatal-error stop state (see header notes).
    bool transportEnabled() const;

    // ---- error observation (UI panel) --------------------------------------
    bool hasError() const;
    PlaybackError::Code lastErrorCode() const;
    /// Empty message when none or unknown.
    const std::string& lastErrorMessage() const;
    /// Entry that failed; empty when none.
    const std::string& lastErrorPath() const;

private:
    PlaybackController& controller_;
    PlaybackQueue& queue_;

    // Snapshot taken by refresh().
    PlaybackController::State state_ = PlaybackController::State::Idle;
    std::string currentEntry_;
    std::size_t currentIndex_ = 0;
    std::size_t queueCount_ = 0;
    long positionSeconds_ = 0;
    long durationSeconds_ = 0;

    PlaybackError::Code errorCode_ = PlaybackError::Code::EngineFailure;
    std::string errorMessage_;
    std::string errorPath_;
    bool hasError_ = false;
};

}  // namespace shanty
