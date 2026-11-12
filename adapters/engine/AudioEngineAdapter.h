// SH-402 (Epic SH-E4): AudioEngineAdapter — thin, framework-bound
// implementation of the core IAudioEngine port.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Backend decision (SH-D402-1, recorded for this ticket; details in
// docs/engine-adapter.md): Sailfish OS ships the `multimedia` Qt module
// (sailfishos/qtmultimedia, Qt 5.6 era) whose QMediaPlayer decodes through
// the GStreamer backend and outputs through the PulseAudio backend — the
// same stack the stock media player uses. The adapter therefore rides
// QMediaPlayer; no native GStreamer or PulseAudio calls of our own.
// The device QMediaPlayer code is guarded by HAVE_QTMULTIMEDIA and is NOT
// compiled on the host (the host build has no QtMultimedia development
// install — see adapters.pro for the qtHaveModule() conditional).
//
// Host/device split (SH-D402-2, recorded for this ticket):
//   Host  (Qt 5.15, no QtMultimedia): the HOST-BUILDABLE subset is
//     QtCore-only — QUrl mapping helpers (D3) + a documented stub variant:
//     construction-safe (no QMediaPlayer is created), transport methods
//     are documented no-ops, state() is always Idle, position/duration are
//     0 / Track::kUnknownDuration, and NO callbacks are ever fired on the
//     host (nothing exists that could fire them).
//   Device (Sailfish SDK, qtHaveModule(multimedia)): full QMediaPlayer
//     wiring. Engine state is observed exclusively through the
//     IAudioEngine-side callbacks (onTrackFinished / onError, which the
//     PlaybackController consumes); all QMediaPlayer signals are translated
//     inside this adapter.
//
// URL mapping (design decision D3, SH-D402-3): the domain deals in plain
// UTF-8 paths only; file:// QUrl conversion happens here, as small public
// static helpers on the adapter:
//   toLocalUrl(plainPath)  — QUrl::fromLocalFile semantics; an empty path
//     yields an empty QUrl (nothing to play).
//   toPlainPath(url)       — QUrl::toLocalFile semantics for local file://
//     URLs, byte-exact for valid UTF-8 paths (spaces, '%' and non-ASCII
//     characters included). A non-local or empty QUrl yields "" (unmappable
//     under D3 — Shanty plays local files only, no streaming).
// Mapping is defined for valid UTF-8 plain paths (D3 premise); non-UTF-8
// byte sequences are NOT round-trip safe and are documented unsupported.
//
// Effect chain (SH-D402-6, decision recorded for the SH-502/SH-E6 Phase B
// wiring): this adapter also implements the IEffectChain port as a
// documented NO-OP/BYPASS. Source of the caveat: docs/engine-adapter.md —
// QtMultimedia on Sailfish (the Qt 5.6-era fork) exposes NO equalizer API
// and QMediaPlayer offers no audio-effect chain (a real EQ would need its
// own GStreamer EQ pipeline or PulseAudio module-equalizer-sink, a backend
// investigation that is out of scope for SH-E6 Phase B). The section
// therefore ACCEPTS EffectChainService calls and DISCARDS them: applying
// gains or a bypass changes no audible behaviour, drives no transport, and
// reports no error — so the SH-605 settings flow (which constructs
// EffectChainService over this chain per apply) works unchanged on the
// device. Applying a profile is a UI no-op until a real EQ backend lands;
// the host stub variant shares the same no-op section.
#pragma once

#include <string>
#include <vector>

#include <QUrl>

#include "core/ports/IAudioEngine.h"
#include "core/ports/IEffectChain.h"

// QtMultimedia, global namespace. Forward-declared unconditionally: the
// private player_ pointer below must be part of ONE class definition for
// every TU (ODR). The host build never constructs the player — see the
// ctor's stub variant (SH-D402-2).
class QMediaPlayer;

namespace shanty {

/// Implements both engine ports: IAudioEngine (transport) and IEffectChain
/// (SH-D402-6 no-op/bypass section — see the decision record above).
class AudioEngineAdapter : public IAudioEngine, public IEffectChain {
public:
    /// Construction is always safe (host stub variant: no backend is
    /// touched; device variant: creates the QMediaPlayer instance).
    AudioEngineAdapter();
    ~AudioEngineAdapter() override;

    // Adapter holds a live media backend (device variant): non-copyable.
    AudioEngineAdapter(const AudioEngineAdapter&) = delete;
    AudioEngineAdapter& operator=(const AudioEngineAdapter&) = delete;

    // ---- IAudioEngine ------------------------------------------------------
    // Host variant: documented no-ops (state stays Idle, no callbacks).
    // Device variant: play() loads the media via toLocalUrl(D3) and starts;
    // state/duration/position updates flow back through the QMediaPlayer
    // signals translated below.
    void play(const std::string& path) override;
    void pause() override;
    void resume() override;
    void stop() override;
    /// SH-806 (UX-10): absolute jump within the current entry. Device
    /// variant: QMediaPlayer::setPosition (Qt 5.6-era milliseconds API).
    /// Host stub variant: documented no-op (SH-D402-2 — nothing exists to
    /// move; no callbacks, state untouched).
    void seek(long positionSeconds) override;
    /// Position within the current entry, in seconds (0 on the host stub).
    long positionSeconds() const override;
    /// Duration of the current entry, Track::kUnknownDuration when unknown
    /// (always unknown on the host stub).
    long durationSeconds() const override;
    State state() const override;

    // ---- D3 URL mapping (pure functions; host-testable, QtCore-only) -------
    /// plain UTF-8 path → local file:// QUrl (empty path → empty QUrl).
    static QUrl toLocalUrl(const std::string& plainPath);
    /// local file:// QUrl → plain UTF-8 path; "" for non-local/empty URLs.
    static std::string toPlainPath(const QUrl& url);

    // ---- IEffectChain (SH-D402-6 no-op/bypass section) ----------------------
    // Both methods accept EffectChainService calls and discard them (no
    // audible effect, no transport side effects, no error). Kept as real
    // overrides so the SH-605 per-apply construction compiles and runs
    // identically on host and device; the backend limitation is the
    // recorded decision, not a stub to be filled here.
    void applyGains(const std::vector<double>& gainsDb) override;
    void bypass() override;

private:
    // ODR note (SH-E6 emulator bring-up): these members are declared
    // UNCONDITIONALLY. An earlier revision guarded them with
    // #ifdef HAVE_QTMULTIMEDIA, giving the host and device variants
    // different object layouts; main's TU then stacked the 48-byte view
    // of this object directly under the 76-byte view the device ctor
    // wrote — durationSeconds_ = -1 landed in the neighbouring
    // PlaylistService hook slot and crashed on the first std::function
    // assignment. The conditional backend wiring lives ONLY in the
    // .cpp now; the layout is identical everywhere.
    QMediaPlayer* player_ = nullptr;

    // Device-only signal translators (defined in the .cpp under the
    // HAVE_QTMULTIMEDIA guard; only they may invoke the public
    // callbacks. Never called on the host stub variant).
    void onPlayerStateChanged(int newState);
    void onPlayerError(int newError, const std::string& errorString);
    void onPlayerPositionChanged(qint64 positionMs);
    void onPlayerDurationChanged(qint64 durationMs);
    void onPlayerMediaChanged();

    std::string currentPath_;  // entry handed to play() (for error reporting)

    // Cached 1-second-granular passthrough state (device variant only).
    long positionSeconds_ = 0;
    long durationSeconds_ = static_cast<long>(-1);  // Track::kUnknownDuration
};

}  // namespace shanty
