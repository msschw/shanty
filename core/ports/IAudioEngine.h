// SH-402 port. Callback-driven (std::function) so the port stays Qt-free
// and engine adapters remain swappable.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <string>

namespace shanty {

struct PlaybackError {
    enum class Code { SkippedUnsupported, DecodeFailure, EngineFailure };

    Code code = Code::EngineFailure;
    std::string path;   ///< entry that failed
    std::string message;
};

class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    enum class State { Idle, Playing, Paused };

    virtual void play(const std::string& path) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
    /// SH-806 (UX-10, port unfreeze — the SH-D604-B1 no-op dies): jumps the
    /// current entry to the given absolute position in seconds. The engine
    /// adapter clamps internally what its backend can honor; the controller
    /// issues the call only from a live transport (Playing/Paused) and the
    /// view-model keeps the SH-D604-B1 clamp contract (clamps into the
    /// snapshot duration before the command surfaces). Implementations
    /// document (and tolerate) a call with nothing loaded as a no-op.
    virtual void seek(long positionSeconds) = 0;
    /// Position within the current entry, in seconds.
    virtual long positionSeconds() const = 0;
    /// Duration of the current entry, Track::kUnknownDuration when unknown.
    virtual long durationSeconds() const = 0;
    virtual State state() const = 0;

    /// Invoked when an entry finished by itself (natural end).
    std::function<void()> onTrackFinished;
    /// Invoked on any playback error; the controller decides the fallback.
    std::function<void(const PlaybackError&)> onError;
};

}  // namespace shanty
