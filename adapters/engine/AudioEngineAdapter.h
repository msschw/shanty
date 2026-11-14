// SH-402 (Epic SH-E4) → SH-903 (Epic SH-E9): AudioEngineAdapter — thin,
// framework-bound implementation of the core IAudioEngine port.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Backend decision (SH-903, updating SH-D402-1; decision record in
// docs/eq-investigation.md): the device engine is GStreamer directly —
// ONE `playbin` element with the 10-band equalizer injected as its
// `audio-filter` (gst equalizer-10bands, ships in
// gstreamer1.0-plugins-good). Verified on the SFOS 5.1 stack by runtime
// probes; QMediaPlayer (the SH-D402-1 QtMultimedia variant) is gone —
// it rode the same gst stack but exposed no equalizer API. The gst core
// headers are the whole compile surface: elements are created by
// factory name at runtime, no plugins-base API is called. Device code
// is guarded by HAVE_GSTREAMER and is NOT compiled on the host (the
// host build has no gst development install — see adapters.pro).
//
// Host/device split (SH-D402-2, unchanged shape):
//   Host  (Qt 5.15, no gst dev): the HOST-BUILDABLE subset is
//     QtCore-only — QUrl mapping helpers (D3) + the pure gst→SH-403
//     error-code mapping (SH-903) + a documented stub variant:
//     construction-safe (no playbin is created), transport methods are
//     documented no-ops, state() is always Idle, position/duration are
//     0 / Track::kUnknownDuration, and NO callbacks are ever fired on
//     the host (nothing exists that could fire them).
//   Device (Sailfish SDK, HAVE_GSTREAMER): full playbin wiring. Engine
//     events flow back exclusively through the IAudioEngine-side
//     callbacks (onTrackFinished / onError, which the
//     PlaybackController consumes); gst bus messages are translated
//     inside this adapter.
//
// GLib main loop integration (SH-903 decision, recorded here): NO
// dedicated GMainLoop thread and NO g_timeout source — bus messages are
// POLLED on the Qt main thread by a 250 ms QTimer (pollBus()). Rationale:
// every gst call and every IAudioEngine callback then runs on ONE
// thread (the Qt main thread that also runs the UI) — no locks, no
// cross-thread callback hazards into PlaybackController, and the port's
// 1-second position granularity is served with margin. EOS latency is
// bounded by the poll interval (imperceptible for track advance).
//
// URL mapping (design decision D3, SH-D402-3, unchanged): the domain
// deals in plain UTF-8 paths only; file:// conversion happens here, as
// small public static helpers on the adapter:
//   toLocalUrl(plainPath)  — QUrl::fromLocalFile semantics; an empty path
//     yields an empty QUrl (nothing to play).
//   toPlainPath(url)       — QUrl::toLocalFile semantics for local file://
//     URLs, byte-exact for valid UTF-8 paths. A non-local or empty QUrl
//     yields "" (unmappable under D3 — Shanty plays local files only).
//   toGstUri(plainPath)    — SH-903: the playbin "uri" property form
//     (fully-encoded file:// URI string; empty path → "").
//
// Effect chain (SH-D402-6, superseded in SH-904): the equalizer element
// is now INJECTED by this adapter (audio-filter) and held as a member;
// applyGains()/bypass() still discard EffectChainService calls in
// SH-903 — the real chain wiring (band mapping + g_object_set) is
// SH-904's ticket.
#pragma once

#include <string>
#include <vector>

#include <QUrl>

#include "core/ports/IAudioEngine.h"
#include "core/ports/IEffectChain.h"

// GStreamer, C API. Forward-declared unconditionally: the pointers below
// must be part of ONE class definition for every TU (ODR). The host build
// never constructs the pipeline — see the ctor's stub variant (SH-D402-2).
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
class QTimer;

namespace shanty {

/// Implements both engine ports: IAudioEngine (transport) and IEffectChain
/// (no-op until SH-904 — see the decision record above).
class AudioEngineAdapter : public IAudioEngine, public IEffectChain {
public:
    /// Construction is always safe (host stub variant: no backend is
    /// touched; device variant: creates the playbin + injects the
    /// equalizer filter; missing pieces degrade per the comments in the
    /// .cpp — construction never fails).
    AudioEngineAdapter();
    ~AudioEngineAdapter() override;

    // Adapter holds a live media backend (device variant): non-copyable.
    AudioEngineAdapter(const AudioEngineAdapter&) = delete;
    AudioEngineAdapter& operator=(const AudioEngineAdapter&) = delete;

    // ---- IAudioEngine ------------------------------------------------------
    // Host variant: documented no-ops (state stays Idle, no callbacks).
    // Device variant: play() hands the file:// URI (D3/toGstUri) to the
    // playbin and drives it to PLAYING; an EMPTY path is a documented
    // no-op (nothing to play). State is answered by live gst state
    // queries; events (EOS / ERROR) surface via pollBus() → callbacks.
    void play(const std::string& path) override;
    void pause() override;
    void resume() override;
    void stop() override;
    /// SH-806 (UX-10): absolute jump within the current entry. Device
    /// variant: flush gst seek (GST_FORMAT_TIME); a call with nothing
    /// loaded is a documented no-op (gst rejects the seek).
    void seek(long positionSeconds) override;
    /// Position within the current entry, in seconds (0 on the host stub,
    /// 0 when gst cannot answer).
    long positionSeconds() const override;
    /// Duration of the current entry, Track::kUnknownDuration when unknown
    /// (gst hasn't reported it / host stub).
    long durationSeconds() const override;
    State state() const override;

    // ---- D3 URL mapping (pure functions; host-testable, QtCore-only) -------
    /// plain UTF-8 path → local file:// QUrl (empty path → empty QUrl).
    static QUrl toLocalUrl(const std::string& plainPath);
    /// local file:// QUrl → plain UTF-8 path; "" for non-local/empty URLs.
    static std::string toPlainPath(const QUrl& url);
    /// SH-903: plain UTF-8 path → fully-encoded file:// URI STRING for the
    /// playbin "uri" property; "" for an empty path.
    static std::string toGstUri(const std::string& plainPath);

    // ---- SH-903: gst bus error → SH-403 code mapping (pure helper) ----------
    /// Maps a GStreamer bus ERROR message's (GError domain string, numeric
    /// code) onto the SH-403 PlaybackError matrix. Pure and gst-header-free
    /// (host-testable); see the decision record in the .cpp.
    static PlaybackError::Code mapGstError(const std::string& domain, int code);

    // ---- IEffectChain (no-op until SH-904) ----------------------------------
    // SH-903 state: the equalizer element is injected and held (see the
    // decision record above) but EffectChainService calls are still
    // DISCARDED here — SH-904 wires the real application (SH-902 band
    // mapping → g_object_set band gains).
    void applyGains(const std::vector<double>& gainsDb) override;
    void bypass() override;

private:
    // ODR note (SH-E6 emulator bring-up): these members are declared
    // UNCONDITIONALLY (host null pointers are part of the layout). An
    // earlier revision guarded members with #ifdef, giving the host and
    // device variants different object layouts — and a crash when the
    // device ctor wrote through the host-sized view. The conditional
    // backend wiring lives ONLY in the .cpp now; the layout is identical
    // everywhere.
    GstElement* playbin_ = nullptr;
    GstElement* equalizer_ = nullptr;  // playbin's audio-filter (SH-E9)
    GstBus* bus_ = nullptr;
    QTimer* busTimer_ = nullptr;  // 250 ms bus poller (Qt main thread)

    // Device-only gst translators (defined in the .cpp under the
    // HAVE_GSTREAMER guard; only they may invoke the public callbacks.
    // Never called on the host stub variant).
    void pollBus();
    void onGstEos();
    void onGstError(const std::string& domain, int code,
                    const std::string& message);

    std::string currentPath_;  // entry handed to play() (for error reporting)

    // Cached 1-second-granular passthrough state (device variant only).
    long positionSeconds_ = 0;
    long durationSeconds_ = static_cast<long>(-1);  // Track::kUnknownDuration
};

}  // namespace shanty
