// SH-403/SH-404 Phase B: PlaybackController implementation. Pure C++ facade
// (D2) owning the transport state machine around PlaybackQueue (SH-401
// stop-at-end) and the IAudioEngine port (SH-402); the SH-403 error fallback
// matrix lives in handleEngineError(). The full documented decision matrix
// (including the exact onInvalidCommand reason strings) is in
// PlaybackController.h and encoded in tst_playback_controller.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "playback/PlaybackController.h"

namespace shanty {

PlaybackController::PlaybackController(PlaybackQueue& queue,
                                       IAudioEngine& engine)
    : queue_(queue), engine_(engine) {
    // The facade drives the engine's callback-driven port depending on it
    // (Dependency Inversion); the adapters call back with engine events only.
    engine_.onTrackFinished = [this] { handleTrackFinished(); };
    engine_.onError = [this](const PlaybackError& error) {
        handleEngineError(error);
    };
}

void PlaybackController::loadQueue(const std::vector<std::string>& paths) {
    if (state_ == State::Playing || state_ == State::Paused) {
        engine_.stop();  // no stale sound from a replaced queue
    }
    queue_.setEntries(paths);  // selection resets to index 0
    clearError();
    restoredFresh_ = false;  // SH-805: a replaced queue is no restored session
    const bool changed = state_ != State::Idle;
    state_ = State::Idle;
    notifyState(changed);
}

void PlaybackController::restoreQueue(const std::vector<std::string>& paths,
                                      std::size_t index, long positionSeconds) {
    if (paths.empty()) {
        // Documented rejection (SH-805): there is no persisted session to
        // adopt — state untouched.
        invalidate("restore: empty queue");
        return;
    }
    if (state_ == State::Playing || state_ == State::Paused) {
        engine_.stop();  // no stale sound under the restored session
    }
    queue_.setEntries(paths);
    // Selection clamps into the restored queue (documented setCurrent rule).
    queue_.setCurrent(index);
    clearError();
    // Restore-paused invariant: the session sits Paused and NOTHING is sent
    // to the engine that starts audio — audibility requires a user command.
    const bool changed = state_ != State::Paused;
    state_ = State::Paused;
    restoredFresh_ = true;
    restoredPositionSeconds_ = positionSeconds;
    notifyState(changed);
}

void PlaybackController::play() {
    if (queue_.empty()) {
        invalidate("play: empty queue");
        return;
    }
    // A fresh explicit play() is a fresh attempt of the current entry: the
    // recorded error is dropped (the entry is replayed, not skipped).
    clearError();
    playCurrent();
}

void PlaybackController::togglePlayPause() {
    switch (state_) {
        case State::Playing:
            pause();
            return;
        case State::Paused:
            resume();
            return;
        case State::Idle:
        case State::Stopped:
            play();  // also routes "empty queue" through play's reason
            return;
    }
}

void PlaybackController::pause() {
    if (state_ != State::Playing) {
        invalidate("pause: not playing");
        return;
    }
    state_ = State::Paused;
    engine_.pause();
    notifyState(true);
}

void PlaybackController::enqueue(const std::string& path) {
    // SH-809 (UX-8): a pure queue-composition change. Deliberately SILENT:
    // no engine call, no state transition, no callback — the shell's 1 Hz
    // tick refreshes the UI snapshot and persists the extended queue (the
    // SH-805 save hook), so enqueue needs no notification of its own.
    if (path.empty()) {
        invalidate("enqueue: empty path");
        return;
    }
    queue_.append(path);
}

void PlaybackController::resume() {
    if (state_ != State::Paused) {
        invalidate("resume: not playing");
        return;
    }
    if (restoredFresh_) {
        // SH-805: the restored session was never started — the engine has
        // nothing to resume. The user's first resume is a fresh attempt of
        // the restored entry (audible), which also drops the display-only
        // restored position.
        playCurrent();
        return;
    }
    state_ = State::Playing;
    engine_.resume();
    notifyState(true);
}

void PlaybackController::stop() {
    if (state_ != State::Playing && state_ != State::Paused) {
        invalidate("stop: nothing playing");
        return;
    }
    state_ = State::Stopped;
    engine_.stop();
    restoredFresh_ = false;  // SH-805: a stopped session reports engine truth
    notifyState(true);
    // The queue selection survives the stop; play() restarts the entry.
}

void PlaybackController::next() {
    if (queue_.empty()) {
        invalidate("next: empty queue");
        return;
    }
    if (!queue_.next()) {  // SH-401: stop-at-end, no wrap-around
        invalidate("next: end of queue");
        return;
    }
    playCurrent();  // transport semantics: the target entry is played
}

void PlaybackController::previous() {
    if (queue_.empty()) {
        invalidate("previous: empty queue");
        return;
    }
    if (!queue_.previous()) {  // SH-401: never a negative position
        invalidate("previous: start of queue");
        return;
    }
    playCurrent();
}

bool PlaybackController::jumpTo(std::size_t index) {
    if (queue_.empty()) {
        invalidate("jumpTo: empty queue");
        return false;
    }
    if (!queue_.jumpTo(index)) {  // SH-401: selection unchanged
        invalidate("jumpTo: index out of range");
        return false;
    }
    playCurrent();  // documented decision: jumpTo plays the target entry
    return true;
}

void PlaybackController::seek(long targetSeconds) {
    if (state_ != State::Playing && state_ != State::Paused) {
        // Documented no-op (SH-806): seek is a transport command — nothing
        // is playing, so the engine is not touched.
        invalidate("seek: nothing playing");
        return;
    }
    // Absolute jump within the current entry; the state is unchanged (seek
    // is not a play/pause/stop). A fresh restored session's display-only
    // offset ends here: the user moved the position deliberately and the
    // engine passthrough (already at the new anchor) takes over.
    restoredFresh_ = false;
    engine_.seek(targetSeconds);
}

PlaybackController::State PlaybackController::state() const { return state_; }

std::string PlaybackController::currentEntry() const { return queue_.current(); }

std::size_t PlaybackController::currentIndex() const {
    return queue_.currentIndex();
}

long PlaybackController::positionSeconds() const {
    // SH-805: a freshly restored session has not played yet — the persisted
    // position is display-only until the first play drops the offset.
    if (restoredFresh_) {
        return restoredPositionSeconds_;
    }
    return engine_.positionSeconds();  // pure passthrough
}

long PlaybackController::durationSeconds() const {
    return engine_.durationSeconds();  // pure passthrough (kUnknownDuration)
}

bool PlaybackController::hasError() const { return hasError_; }

const PlaybackError& PlaybackController::lastError() const {
    return lastError_;
}

void PlaybackController::clearError() { hasError_ = false; }

void PlaybackController::handleTrackFinished() {
    advanceOrStop();  // end of track follows the SH-401 stop-at-end semantics
}

void PlaybackController::handleEngineError(const PlaybackError& error) {
    recordError(error);
    // SH-403 documented fallback matrix:
    //   SkippedUnsupported → controlled skip (advance to the next entry,
    //     stop at the end) — unsupported files never crash or kill playback;
    //   DecodeFailure / EngineFailure → FATAL: stop, no auto-skip (the user
    //     decides whether to replay the entry), error stays recorded.
    if (error.code == PlaybackError::Code::SkippedUnsupported) {
        advanceOrStop();
        return;
    }
    state_ = State::Stopped;
    engine_.stop();
    notifyState(true);
}

void PlaybackController::recordError(const PlaybackError& error) {
    lastError_ = error;
    hasError_ = true;
}

void PlaybackController::notifyState(bool changed) {
    if (changed && onStateChanged) {
        onStateChanged();
    }
}

void PlaybackController::invalidate(const char* reason) {
    // Documented no-op: the command leaves queue/engine/state untouched; the
    // reason is only observable through the hook.
    if (onInvalidCommand) {
        onInvalidCommand(reason);
    }
}

void PlaybackController::playCurrent() {
    restoredFresh_ = false;  // SH-805: the restored offset ends with a play
    const bool changed = state_ != State::Playing;
    state_ = State::Playing;
    engine_.play(queue_.current());
    notifyState(changed);
}

void PlaybackController::advanceOrStop() {
    if (queue_.empty()) {
        invalidate("next: empty queue");
        return;
    }
    if (queue_.next()) {
        playCurrent();  // gapless advance onto the following entry
        return;
    }
    // Last entry (SH-401 stop-at-end): terminal stop, no wrap-around.
    state_ = State::Stopped;
    engine_.stop();
    notifyState(true);
}

}  // namespace shanty
