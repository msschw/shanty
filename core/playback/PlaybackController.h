// SH-404: application-layer playback facade (design D2 — the engine adapter
// only receives "play this entry now"; all transport decisions live in core).
// Depends on PlaybackQueue (SH-401, stop-at-end semantics) and the
// IAudioEngine port (SH-402, SH-403 error fallback).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Documented decision matrix (SH-403/SH-404; encoded in tst_playback_controller):
//   State machine (facade-owned; the engine's own state is NOT queried):
//     Idle    — initial state / after loadQueue(): queue selected, not playing.
//     Playing — engine receives play(currentEntry).
//     Paused  — engine receives pause()/resume() around it.
//     Stopped — explicit stop(), terminal stop at the last queue entry, or a
//               fatal (non-skippable) playback error.
//   - loadQueue(): queue.setEntries(paths) (selection → index 0), clears the
//     recorded error, state → Idle; a Playing/Paused engine is stopped first.
//   - play(): no-op + onInvalidCommand("play: empty queue") when the queue is
//     empty; otherwise (re)issues engine.play(currentEntry), state → Playing,
//     and clears the recorded error (a fresh play is a fresh attempt).
//   - togglePlayPause(): Idle/Stopped → play(); Playing → pause();
//     Paused → resume(); on an empty queue → onInvalidCommand and no state
//     change.
//   - pause()/resume(): valid only from Playing / Paused respectively;
//     otherwise onInvalidCommand and no engine call.
//   - stop(): valid from Playing/Paused → engine.stop(), state → Stopped;
//     from Idle/Stopped → onInvalidCommand (nothing to stop).
//   - End of track (engine onTrackFinished): queue.next() → on success
//     engine.play(nextEntry) and state stays Playing (gapless advance);
//     at the LAST entry the SH-401 stop-at-end semantics apply — NO wrap:
//     engine.stop(), state → Stopped.
//   - Error fallback (SH-403): the error is always recorded
//     (lastError()/hasError() for observation), then
//       SkippedUnsupported → controlled skip: advance like end-of-track
//         (engine.play(next); at the last entry → stop, no wrap);
//       DecodeFailure / EngineFailure → FATAL, no auto-skip (documented
//         decision): engine.stop(), state → Stopped, error stays recorded
//         until the next play()/loadQueue().
//   - next()/previous(): valid when the queue is non-empty and the edge
//     allows it (SH-401 semantics); the target entry is selected AND played
//     (transport semantics: state → Playing). Beyond the edge or on an empty
//     queue → onInvalidCommand, no engine call, state unchanged.
//   - jumpTo(index): empty queue or out-of-range index → onInvalidCommand
//     and false (selection unchanged); a valid index is selected AND played
//     → true.
//   - positionSeconds()/durationSeconds(): pure passthrough from the engine;
//     durationSeconds() is Track::kUnknownDuration while unknown.
//   - Observation hooks (pure C++ callbacks, like the IAudioEngine port —
//     core stays Qt-free and exception-free; no exceptions for flow control):
//       onStateChanged()  — fired after every actual state transition.
//       onInvalidCommand(const char* reason) — fired for commands that cannot
//         be executed on the current state/queue (no-op), so callers can
//         surface a UI hint without exceptions. Reason catalogue (asserted
//         in the suite, kept stable for the QML layer):
//           "play: empty queue" · "next: empty queue" · "next: end of queue"
//           "previous: empty queue" · "previous: start of queue"
//           "jumpTo: empty queue" · "jumpTo: index out of range"
//           "pause: not playing" · "resume: not playing"
//           "stop: nothing playing"
//         (togglePlayPause() on an empty queue routes through play() →
//         "play: empty queue".)
//   - Session restore (SH-805, UX-4): restoreQueue(paths, index, position)
//     adopts a persisted session PAUSED at the clamped index and issues NO
//     engine call that starts audio (restore-paused invariant — a restored
//     session never starts audio on its own). Until the first play the
//     persisted position is DISPLAY-ONLY (positionSeconds() reports it);
//     the first play (resume/toggle on the fresh session, next/previous/
//     jumpTo/play) drops the offset and reverts to the engine passthrough,
//     as do stop() and loadQueue() (the restored session ended). A fresh
//     restore's resume() issues engine.play (fresh attempt) — not
//     engine.resume() — so the user's first tap on a restored session is
//     audible. Empty paths → onInvalidCommand("restore: empty queue").
//   - Seek (SH-806, UX-10 — SH-D604-B1's deliberate no-op dies): seek(t)
//     from Playing/Paused issues engine.seek(t) (absolute jump; the engine
//     adapter owns backend specifics) and leaves the state unchanged; from
//     Idle/Stopped it is a documented no-op with
//     onInvalidCommand("seek: nothing playing") and no engine call. The
//     VM-side clamp into the snapshot duration (SH-D604-B1 contract) is
//     unchanged — the controller never re-clamps. A seek on a fresh
//     restored session drops the restored display offset (the user moved
//     the position deliberately; the engine passthrough takes over).
//
// This facade is pure C++ (D1): the QObject/QML wrapper is a later layer and
// must hold a reference to this class only.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "domain/Track.h"
#include "playback/PlaybackQueue.h"
#include "ports/IAudioEngine.h"

namespace shanty {

class PlaybackController {
public:
    enum class State { Idle, Playing, Paused, Stopped };

    explicit PlaybackController(PlaybackQueue& queue, IAudioEngine& engine);

    // ---- observation hooks -------------------------------------------------
    /// Fired after every actual state transition (not on redundant commands).
    std::function<void()> onStateChanged;
    /// Fired for a command that was rejected as a documented no-op.
    std::function<void(const char* reason)> onInvalidCommand;

    // ---- queue + transport -------------------------------------------------
    /// Replaces the queue contents (selection → index 0); does NOT auto-play.
    void loadQueue(const std::vector<std::string>& paths);

    /// SH-805 (UX-4): restores a persisted session PAUSED at the saved
    /// track — replaces the queue with the given paths (given order),
    /// selects the clamped index, and issues NO engine call that starts
    /// audio (restore-paused invariant: a restored session never starts
    /// audio on its own). Until the first play, positionSeconds() reports
    /// the restored position (display-only — the engine has not played);
    /// the first user-initiated play (resume/toggle/next/previous/jumpTo)
    /// drops the restored offset and reverts to the engine passthrough.
    /// Empty paths → documented rejection "restore: empty queue" (state
    /// untouched). See the header decision notes.
    void restoreQueue(const std::vector<std::string>& paths,
                      std::size_t index, long positionSeconds);

    /// (Re)starts playback of the current entry.
    void play();
    /// SH-809 (UX-8): appends one path at the END of the current queue —
    /// a PURE composition change: no engine call, no state transition, no
    /// callback (the 1 Hz shell tick refreshes the UI and persists the
    /// extended queue). Empty path → documented rejection
    /// "enqueue: empty path" (queue and state untouched).
    void enqueue(const std::string& path);
    /// Idle/Stopped → play, Playing → pause, Paused → resume.
    void togglePlayPause();
    void pause();
    void resume();
    void stop();

    /// Selects and plays the following entry; no-op beyond the last entry.
    void next();
    /// Selects and plays the preceding entry; no-op before the first entry.
    void previous();
    /// Selects (jumpTo) and plays the index; returns false when rejected.
    bool jumpTo(std::size_t index);

    /// SH-806 (UX-10): the seek command — issues engine.seek(target) from a
    /// live transport (Playing/Paused) and is a documented no-op otherwise
    /// ("seek: nothing playing"). The SH-D604-B1 clamping contract stays in
    /// the view-model (clamps into the snapshot duration before the command
    /// surfaces); a seek during a fresh restored session drops the
    /// display-only restored offset (the user's explicit position change
    /// ends it). See the header decision notes.
    void seek(long targetSeconds);

    // ---- observable state --------------------------------------------------
    State state() const;
    /// Empty string when nothing is selected (no queue / empty queue).
    std::string currentEntry() const;
    std::size_t currentIndex() const;

    /// Passthrough from the engine, in seconds.
    long positionSeconds() const;
    /// Passthrough from the engine; Track::kUnknownDuration when unknown.
    long durationSeconds() const;

    // ---- error observation (SH-403) ----------------------------------------
    bool hasError() const;
    /// The recorded error of the last failed playback (stopped/skipped path).
    const PlaybackError& lastError() const;
    /// Drops the recorded error (also implied by play()/loadQueue()).
    void clearError();

private:
    PlaybackQueue& queue_;
    IAudioEngine& engine_;

    State state_ = State::Idle;
    PlaybackError lastError_;
    bool hasError_ = false;

    // SH-805 (UX-4) restore state: a freshly restored session is PAUSED and
    // has never played — positionSeconds() reports the persisted position
    // (display-only) until the first play/stop/loadQueue drops the flag.
    bool restoredFresh_ = false;
    long restoredPositionSeconds_ = 0;

    void handleTrackFinished();
    void handleEngineError(const PlaybackError& error);

    void recordError(const PlaybackError& error);
    void notifyState(bool changed);
    void invalidate(const char* reason);
    /// Issues engine.play for the currently selected entry; state → Playing.
    void playCurrent();
    /// Shared end-of-track / controlled-skip advance (SH-401 stop-at-end).
    void advanceOrStop();
};

}  // namespace shanty
