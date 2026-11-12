// SH-402 (Epic SH-E4): AudioEngineAdapter.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Host/device split (SH-D402-2): the host build (no QtMultimedia dev
// install) compiles ONLY the QtCore-only stub variant below; the full
// QMediaPlayer device wiring is guarded by HAVE_QTMULTIMEDIA and built by
// the Sailfish SDK target (see adapters/adapters.pro). No exceptions —
// failures surface through the IAudioEngine callbacks (port contract).
#include "adapters/engine/AudioEngineAdapter.h"

#include "core/domain/Track.h"

#ifdef HAVE_QTMULTIMEDIA
#include <QObject>
#include <QMediaPlayer>
#endif

namespace shanty {

AudioEngineAdapter::AudioEngineAdapter() {
#ifdef HAVE_QTMULTIMEDIA
    // SH-D402-1: full backend wiring (GStreamer decode → PulseAudio output
    // per docs/engine-adapter.md).
    player_ = new QMediaPlayer();
    QObject::connect(player_, &QMediaPlayer::stateChanged, player_,
            [this](QMediaPlayer::State newState) {
                onPlayerStateChanged(static_cast<int>(newState));
            });
    QObject::connect(player_,
            static_cast<void (QMediaPlayer::*)(QMediaPlayer::Error)>(
                &QMediaPlayer::error),
            player_, [this](QMediaPlayer::Error newError) {
                onPlayerError(static_cast<int>(newError),
                              player_->errorString().toStdString());
            });
    QObject::connect(player_, &QMediaPlayer::positionChanged, player_,
            [this](qint64 positionMs) {
                onPlayerPositionChanged(positionMs);
            });
    QObject::connect(player_, &QMediaPlayer::durationChanged, player_,
            [this](qint64 durationMs) {
                onPlayerDurationChanged(durationMs);
            });
    QObject::connect(player_, &QMediaPlayer::mediaChanged, player_,
            [this](const QMediaContent&) { onPlayerMediaChanged(); });
    // No other callbacks (onTrackFinished/onError) are fired before any
    // command: they are invoked exclusively by the translators above.
#endif
}

AudioEngineAdapter::~AudioEngineAdapter() {
#ifdef HAVE_QTMULTIMEDIA
    delete player_;
    player_ = nullptr;
#endif
}

// ---- IAudioEngine ----------------------------------------------------------

void AudioEngineAdapter::play(const std::string& path) {
#ifdef HAVE_QTMULTIMEDIA
    // D3: the plain path becomes a local file:// URL at this boundary only.
    currentPath_ = path;
    player_->setMedia(AudioEngineAdapter::toLocalUrl(path));
    player_->play();
#else
    // Documented host stub variant: transport is a no-op. The engine state
    // on the host is only ever Idle; observe callbacks never fire here.
    (void)path;  // SH-D402-2
#endif
}

void AudioEngineAdapter::pause() {
#ifdef HAVE_QTMULTIMEDIA
    player_->pause();
#else
    // Documented host stub variant: no-op (SH-D402-2).
#endif
}

void AudioEngineAdapter::resume() {
#ifdef HAVE_QTMULTIMEDIA
    if (player_->state() == QMediaPlayer::PausedState) {
        player_->play();
    }
#else
    // Documented host stub variant: no-op (SH-D402-2).
#endif
}

void AudioEngineAdapter::stop() {
#ifdef HAVE_QTMULTIMEDIA
    player_->stop();
#else
    // Documented host stub variant: no-op (SH-D402-2).
#endif
}

void AudioEngineAdapter::seek(long positionSeconds) {
#ifdef HAVE_QTMULTIMEDIA
    // SH-806 (UX-10): absolute jump within the current entry. Qt 5.6-era
    // API: setPosition(milliseconds). A call with nothing loaded is a
    // documented no-op of the backend itself (no media → no position).
    player_->setPosition(static_cast<qint64>(positionSeconds) * 1000);
#else
    // Documented host stub variant: no-op (SH-D402-2).
    (void)positionSeconds;
#endif
}

long AudioEngineAdapter::positionSeconds() const {
    return positionSeconds_;  // 0 on the host stub (SH-D402-2)
}

long AudioEngineAdapter::durationSeconds() const {
    // Track::kUnknownDuration while unknown; always unknown on the host stub.
    return durationSeconds_;
}

IAudioEngine::State AudioEngineAdapter::state() const {
#ifdef HAVE_QTMULTIMEDIA
    switch (player_->state()) {
        case QMediaPlayer::PlayingState: return State::Playing;
        case QMediaPlayer::PausedState: return State::Paused;
        case QMediaPlayer::StoppedState: break;
    }
#endif
    return State::Idle;
}

// ---- D3 URL mapping (SH-402 Phase B, SH-D402-3) ----------------------------
// Exactly QUrl::fromLocalFile / toLocalFile semantics at this boundary (D3:
// the domain sees plain UTF-8 paths only; file:// conversion lives here).
// Callbacks, not exceptions: mapping cannot fail — unmappable inputs or
// nothing-to-play produce the documented empty values ("" / empty QUrl).
// Byte-exact round trip is defined for valid UTF-8 plain paths (D3 premise;
// non-UTF-8 byte sequences are documented unsupported — see the header).

QUrl AudioEngineAdapter::toLocalUrl(const std::string& plainPath) {
    if (plainPath.empty()) {
        return QUrl();  // nothing to play (documented value)
    }
    return QUrl::fromLocalFile(
        QString::fromUtf8(plainPath.data(),
                          static_cast<int>(plainPath.size())));
}

std::string AudioEngineAdapter::toPlainPath(const QUrl& url) {
    if (!url.isLocalFile()) {
        return std::string();  // non-local/empty URLs are unmappable under D3
    }
    return url.toLocalFile().toStdString();  // percent-decoded UTF-8
}

// ---- IEffectChain (SH-D402-6 no-op/bypass section) --------------------------
// Documented no-op on BOTH host and device variants (see the decision record
// in the header): QtMultimedia on Sailfish has no equalizer API to wire, so
// the chain accepts commands and discards them. EffectChainService's
// belt-and-braces bypass sequence (bypass() + explicit zero gains) therefore
// completes without any backend effect and without touching the transport.

void AudioEngineAdapter::applyGains(const std::vector<double>& gainsDb) {
    (void)gainsDb;  // no backend effect chain available (SH-D402-6)
}

void AudioEngineAdapter::bypass() {
    // No backend effect chain available (SH-D402-6): nothing to bypass.
}

#ifdef HAVE_QTMULTIMEDIA
// ---- device-only signal translators (SH-D402-2) ----------------------------
// These are the ONLY functions that fire the public callbacks. Recorded
// decisions:
//   SH-D402-4: error code mapping (see header/docs)
//     FormatError → SkippedUnsupported · ResourceError/AccessDeniedError →
//     DecodeFailure · ServiceMissing/other → EngineFailure.
//   SH-D402-5: natural end-of-track is Detected ONLY as
//     StoppedState + mediaStatus == EndOfMedia (any stop/pause/media switch
//     also visits StoppedState and must NOT look like a finished track).

void AudioEngineAdapter::onPlayerStateChanged(int newState) {
    if (newState == static_cast<int>(QMediaPlayer::StoppedState) &&
        player_->mediaStatus() == QMediaPlayer::EndOfMedia) {
        // Natural end only: Play→Stopped visits Stopped too (every
        // pause/stop/media switch), so this branch requires EndOfMedia.
        positionSeconds_ = 0;
        if (onTrackFinished) {
            onTrackFinished();
        }
        return;  // state() answer below stays Idle (StoppedState).
    }
    positionSeconds_ = 0;  // new state context: position restarts
}

void AudioEngineAdapter::onPlayerError(int newError,
                                       const std::string& errorString) {
    PlaybackError::Code code;
    switch (newError) {
        case QMediaPlayer::FormatError:
            code = PlaybackError::Code::SkippedUnsupported;
            break;
        case QMediaPlayer::ResourceError:
        case QMediaPlayer::AccessDeniedError:
            code = PlaybackError::Code::DecodeFailure;
            break;
        default:
            code = PlaybackError::Code::EngineFailure;
            break;
    }
    if (onError) {
        onError(PlaybackError{code, currentPath_, errorString});
    }
}

void AudioEngineAdapter::onPlayerPositionChanged(qint64 positionMs) {
    positionSeconds_ = static_cast<long>(positionMs / 1000);
}

void AudioEngineAdapter::onPlayerDurationChanged(qint64 durationMs) {
    durationSeconds_ = static_cast<long>(durationMs / 1000);
}

void AudioEngineAdapter::onPlayerMediaChanged() {
    // New media handed to play(): position restarts, duration unknown until
    // the backend reports it.
    positionSeconds_ = 0;
    durationSeconds_ = Track::kUnknownDuration;
}
#endif

}  // namespace shanty
