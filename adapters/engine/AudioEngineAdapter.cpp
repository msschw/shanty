// SH-402 (Epic SH-E4) → SH-903 (Epic SH-E9): AudioEngineAdapter.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host/device split (SH-D402-2): the host build (no gst dev install)
// compiles ONLY the QtCore-only stub variant below; the full playbin
// device wiring is guarded by HAVE_GSTREAMER and built by the Sailfish
// SDK target (see adapters/adapters.pro). No exceptions — failures
// surface through the IAudioEngine callbacks (port contract).
#include "adapters/engine/AudioEngineAdapter.h"

#include "core/domain/Track.h"

#ifdef HAVE_GSTREAMER
#include <QTimer>
#include <gst/gst.h>
#endif

namespace shanty {

AudioEngineAdapter::AudioEngineAdapter() {
#ifdef HAVE_GSTREAMER
    // gst_init must run before ANY gst call (factory_make asserts
    // gst_is_initialized — caught on the emulator's first drive, SH-903).
    // argc/argv are left untouched: gst_init(nullptr, nullptr) only
    // removes gst-own options, and no gst flags are used here.
    gst_init(nullptr, nullptr);
    // SH-903: one playbin; the 10-band equalizer rides it as the
    // audio-filter (SH-E9, docs/eq-investigation.md). A missing playbin
    // or equalizer factory degrades gracefully: playbin absent → the
    // adapter stays inert (state Idle, no callbacks, transport no-ops);
    // equalizer absent (plugin not shipped) → playbin plays WITHOUT the
    // filter — the engine still works, only the EQ feature is missing.
    playbin_ = gst_element_factory_make("playbin", "shanty-playbin");
    if (!playbin_)
        return;
    equalizer_ =
        gst_element_factory_make("equalizer-10bands", "shanty-equalizer");
    if (equalizer_) {
        // playbin takes ownership (floating ref absorbed).
        g_object_set(playbin_, "audio-filter", equalizer_, nullptr);
    }
    bus_ = gst_element_get_bus(playbin_);
    // SH-903 GLib-integration decision (see header): Qt-main-thread bus
    // polling, no GMainLoop thread — everything runs on one thread.
    busTimer_ = new QTimer();
    QObject::connect(busTimer_, &QTimer::timeout, busTimer_,
                     [this] { pollBus(); });
    busTimer_->start(250);
#endif
}

AudioEngineAdapter::~AudioEngineAdapter() {
#ifdef HAVE_GSTREAMER
    delete busTimer_;
    busTimer_ = nullptr;
    if (bus_) {
        gst_object_unref(bus_);
        bus_ = nullptr;
    }
    if (playbin_) {
        gst_element_set_state(playbin_, GST_STATE_NULL);
        gst_object_unref(playbin_);  // tears down the pipeline + filter
        playbin_ = nullptr;
        equalizer_ = nullptr;  // owned (and destroyed) by the playbin
    }
#endif
}

// ---- IAudioEngine ----------------------------------------------------------

void AudioEngineAdapter::play(const std::string& path) {
#ifdef HAVE_GSTREAMER
    if (!playbin_)
        return;
    // D3: the plain path becomes a fully-encoded file:// URI at this
    // boundary only. An empty path is a documented no-op (nothing to
    // play) — the pipeline is untouched.
    const std::string uri = toGstUri(path);
    if (uri.empty()) {
        return;
    }
    currentPath_ = path;
    g_object_set(playbin_, "uri", uri.c_str(), nullptr);
    gst_element_set_state(playbin_, GST_STATE_PLAYING);
#else
    // Documented host stub variant: transport is a no-op. The engine state
    // on the host is only ever Idle; observe callbacks never fire here.
    (void)path;  // SH-D402-2
#endif
}

void AudioEngineAdapter::pause() {
#ifdef HAVE_GSTREAMER
    if (playbin_)
        gst_element_set_state(playbin_, GST_STATE_PAUSED);
#else
    // Documented host stub variant: no-op (SH-D402-2).
#endif
}

void AudioEngineAdapter::resume() {
#ifdef HAVE_GSTREAMER
    if (!playbin_)
        return;
    // SH-D402-2 shape kept from the Qt variant: resume only continues a
    // PAUSED transport — it must not restart a stopped one from zero.
    GstState current = GST_STATE_NULL;
    gst_element_get_state(playbin_, &current, nullptr, 0);
    if (current == GST_STATE_PAUSED) {
        gst_element_set_state(playbin_, GST_STATE_PLAYING);
    }
#else
    // Documented host stub variant: no-op (SH-D402-2).
#endif
}

void AudioEngineAdapter::stop() {
#ifdef HAVE_GSTREAMER
    // READY resets the stream position and keeps the pipeline object
    // (cheap re-play of the next track without re-creating anything).
    if (playbin_)
        gst_element_set_state(playbin_, GST_STATE_READY);
#else
    // Documented host stub variant: no-op (SH-D402-2).
#endif
}

void AudioEngineAdapter::seek(long positionSeconds) {
#ifdef HAVE_GSTREAMER
    if (!playbin_)
        return;
    // SH-806 (UX-10): absolute jump within the current entry, flushed.
    // GST_SEEK_FLAG_KEY_UNIT: the sink may start from the nearest key
    // frame — audio-only streams have no visual key-frame constraint.
    // A call with nothing loaded is a documented no-op (gst rejects the
    // seek when no media is loaded); nothing here can crash.
    gst_element_seek_simple(playbin_, GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                                      GST_SEEK_FLAG_KEY_UNIT),
                            static_cast<gint64>(positionSeconds) *
                                GST_SECOND);
#else
    // Documented host stub variant: no-op (SH-D402-2).
    (void)positionSeconds;
#endif
}

long AudioEngineAdapter::positionSeconds() const {
#ifdef HAVE_GSTREAMER
    gint64 positionNs = 0;
    if (playbin_ &&
        gst_element_query_position(playbin_, GST_FORMAT_TIME, &positionNs) &&
        positionNs > 0) {
        return static_cast<long>(positionNs / GST_SECOND);
    }
    return 0;
#else
    return positionSeconds_;  // 0 on the host stub (SH-D402-2)
#endif
}

long AudioEngineAdapter::durationSeconds() const {
#ifdef HAVE_GSTREAMER
    gint64 durationNs = 0;
    if (playbin_ &&
        gst_element_query_duration(playbin_, GST_FORMAT_TIME, &durationNs) &&
        durationNs > 0) {
        return static_cast<long>(durationNs / GST_SECOND);
    }
#endif
    // Track::kUnknownDuration while unknown; always unknown on the host stub.
    return static_cast<long>(Track::kUnknownDuration);
}

IAudioEngine::State AudioEngineAdapter::state() const {
#ifdef HAVE_GSTREAMER
    GstState current = GST_STATE_NULL;
    if (playbin_ &&
        gst_element_get_state(playbin_, &current, nullptr, 0) !=
            GST_STATE_CHANGE_FAILURE) {
        switch (current) {
            case GST_STATE_PLAYING: return State::Playing;
            case GST_STATE_PAUSED:  return State::Paused;
            default:                break;  // NULL/READY/VOID_PENDING → Idle
        }
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

std::string AudioEngineAdapter::toGstUri(const std::string& plainPath) {
    // Fully-encoded file:// URI string for the playbin "uri" property
    // (spaces/'%'/non-ASCII percent-encoded by QUrl::FullyEncoded);
    // an empty path yields "" (nothing to play — documented no-op).
    return toLocalUrl(plainPath)
        .toString(QUrl::FullyEncoded)
        .toStdString();
}

// ---- SH-903: gst bus error → SH-403 code mapping ----------------------------
// Pure and gst-header-free: (GError domain string, numeric code) → the
// SH-403 PlaybackError matrix. Decisions (recorded; the numeric codes are
// the gst enum values from gstreamer's error headers, verified against
// gstcoreerror.h / gststreamerror.h):
//   - "gst-core-error" MISSING_PLUGIN (14) → SkippedUnsupported: the
//     decoder/plugin for the format is absent — exactly the format gate.
//   - "gst-stream-error" CODEC_NOT_FOUND (6) → SkippedUnsupported: same
//     surface, codec side.
//   - "gst-resource-error" (any code) → DecodeFailure: the file exists
//     but cannot be read/opened (NOT_FOUND, READ, ...).
//   - "gst-stream-error" (any other code) → DecodeFailure: demux/decode/
//     format errors on an otherwise readable stream.
//   - "gst-library-error" and ANY unknown domain → EngineFailure
//     (belt: never misreport a non-format failure as "unsupported").

PlaybackError::Code AudioEngineAdapter::mapGstError(const std::string& domain,
                                                    int code) {
    if (domain == "gst-core-error" && code == 14) {  // MISSING_PLUGIN
        return PlaybackError::Code::SkippedUnsupported;
    }
    if (domain == "gst-stream-error" && code == 6) {  // CODEC_NOT_FOUND
        return PlaybackError::Code::SkippedUnsupported;
    }
    if (domain == "gst-resource-error") {
        return PlaybackError::Code::DecodeFailure;
    }
    if (domain == "gst-stream-error") {
        return PlaybackError::Code::DecodeFailure;
    }
    return PlaybackError::Code::EngineFailure;
}

// ---- IEffectChain (SH-903: injected element, discarded calls) ---------------
// The equalizer element is injected into the playbin (audio-filter) and
// held as equalizer_; SH-903 still DISCARDS EffectChainService calls —
// the real application (SH-902 mapping → band gains via g_object_set)
// lands in SH-904. Kept as real overrides so the SH-605 per-apply
// construction compiles and runs identically on host and device.

void AudioEngineAdapter::applyGains(const std::vector<double>& gainsDb) {
    (void)gainsDb;  // chain wiring is SH-904
}

void AudioEngineAdapter::bypass() {
    // chain wiring is SH-904
}

#ifdef HAVE_GSTREAMER
// ---- device-only gst translators (SH-903) -----------------------------------
// These are the ONLY functions that fire the public callbacks. Recorded
// decisions:
//   SH-D402-5 (kept in kind): natural end-of-track is detected ONLY as
//     GST_MESSAGE_EOS on the playbin bus (any stop/pause/uri switch also
//     moves state and must NOT look like a finished track).
//   SH-D402-4 (kept in kind, gst surface): error codes map via the pure
//     mapGstError() helper (see its decision record above).

void AudioEngineAdapter::pollBus() {
    if (!bus_)
        return;
    GstMessage* message = nullptr;
    while ((message = gst_bus_pop(bus_)) != nullptr) {
        switch (GST_MESSAGE_TYPE(message)) {
            case GST_MESSAGE_EOS:
                gst_message_unref(message);
                onGstEos();
                return;  // callbacks may re-enter the transport; drain later
            case GST_MESSAGE_ERROR: {
                GError* rawError = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_error(message, &rawError, &debug);
                if (rawError) {
                    // GError::domain is a GQuark — the string form is what
                    // mapGstError() matches against.
                    const std::string domain = rawError->domain
                                                   ? g_quark_to_string(rawError->domain)
                                                   : "";
                    onGstError(domain, rawError->code,
                               rawError->message ? rawError->message : "");
                    g_error_free(rawError);
                }
                g_free(debug);
                gst_message_unref(message);
                return;
            }
            default:
                break;  // state-changed/tag/async-done etc. are not observed
        }
        gst_message_unref(message);
    }
}

void AudioEngineAdapter::onGstEos() {
    // Natural end only: the stream ran to its end by itself.
    positionSeconds_ = 0;
    if (onTrackFinished) {
        onTrackFinished();
    }
}

void AudioEngineAdapter::onGstError(const std::string& domain, int code,
                                    const std::string& message) {
    // gst contract: after a pipeline ERROR the playbin is POISONED — a
    // subsequent state change fails with "state change failed and some
    // element failed to post a proper error message" (observed on the
    // emulator: an errored corrupt-sample play made the NEXT play()
    // fail even with a perfectly good file, and "Try again" kept
    // failing; the identical playbin+filter pipeline played the same
    // FLAC fine from gst-launch). gst places the reset responsibility
    // on the application: drive the playbin back to NULL, which tears
    // the errored decode graph down; the next play() re-prerolls it.
    if (playbin_)
        gst_element_set_state(playbin_, GST_STATE_NULL);
    positionSeconds_ = 0;  // new state context: position restarts
    if (onError) {
        onError(PlaybackError{mapGstError(domain, code), currentPath_, message});
    }
}
#endif

}  // namespace shanty
