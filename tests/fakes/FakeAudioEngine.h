// Shared test double for SH-403/SH-404: a recording IAudioEngine. Records
// play/pause/resume/stop calls with arguments, exposes controllable
// position/duration/state, and lets the test trigger onTrackFinished() /
// onError(PlaybackError{...}) manually to assert callback-driven state
// transitions of the PlaybackController.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/IAudioEngine.h"
#include "domain/Track.h"

class FakeAudioEngine : public shanty::IAudioEngine {
public:
    // ---- recorded calls ----------------------------------------------------
    /// Paths handed to play(), in call order (empty when never played).
    std::vector<std::string> playCalls;
    int pauseCalls = 0;
    int resumeCalls = 0;
    int stopCalls = 0;

    // ---- controllable passthrough state ------------------------------------
    long posSeconds = 0;
    long durSeconds = shanty::Track::kUnknownDuration;

    // ---- IAudioEngine ------------------------------------------------------
    void play(const std::string& path) override {
        playCalls.push_back(path);
        state_ = State::Playing;
    }

    void pause() override {
        ++pauseCalls;
        if (state_ == State::Playing) {
            state_ = State::Paused;
        }
    }

    void resume() override {
        ++resumeCalls;
        if (state_ == State::Paused) {
            state_ = State::Playing;
        }
    }

    void stop() override {
        ++stopCalls;
        state_ = State::Idle;
    }

    long positionSeconds() const override { return posSeconds; }
    long durationSeconds() const override { return durSeconds; }
    State state() const override { return state_; }

    // ---- test triggers (the controller wires the callbacks on construction;
    // tests must trigger through these helpers, not by assigning the hooks,
    // so nothing is overwritten) --------------------------------------------
    void finishTrack() {
        if (onTrackFinished) {
            onTrackFinished();
        }
    }

    void emitError(shanty::PlaybackError::Code code, const std::string& path,
                   const std::string& message) {
        if (onError) {
            onError(shanty::PlaybackError{code, path, message});
        }
    }

private:
    State state_ = State::Idle;
};
