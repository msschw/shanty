// SH-402 Phase A: AudioEngineAdapter host suite.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Scope is deliberately limited (per ticket): (1) adapter-construction
// sanity — default state Idle and no callbacks fired before any command —
// and (2) URL-mapping sanity per design decision D3: plain path ⇄ local
// file:// QUrl mapping (QUrl::fromLocalFile/toLocalFile semantics incl.
// spaces/unusual bytes round trip) via the public static helpers
// toLocalUrl()/toPlainPath(). Device-specific sound output is guarded by
// HAVE_QTMULTIMEDIA and is validated manually on the emulator/device
// (docs/engine-adapter.md). Phase A ran against the SH-402 stubs: every
// unimplemented case failed by ASSERTION (QFATAL count 0), never by crash.
#include <QtTest>

#include <QUrl>

#include "core/ports/IAudioEngine.h"
#include "domain/Track.h"
#include "adapters/engine/AudioEngineAdapter.h"

using shanty::AudioEngineAdapter;
using shanty::IAudioEngine;
using shanty::PlaybackError;

namespace {

/// Construction-sanity fixture: fresh adapter with observation counters for
/// the port-side callbacks (on the host stub variant these are expected to
/// never fire at all — SH-D402-2).
struct ConstructorProbe {
    AudioEngineAdapter adapter;

    int finishFired = 0;
    int errorFired = 0;
    PlaybackError lastError;

    ConstructorProbe() {
        adapter.onTrackFinished = [this]() { ++finishFired; };
        adapter.onError = [this](const PlaybackError& error) {
            ++errorFired;
            lastError = error;
        };
        // Note: no command is issued here — construction sanity asserts that
        // NOTHING happens before the first command.
    }
};

}  // namespace

class TestEngineAdapter : public QObject {
    Q_OBJECT

private slots:
    // ---- construction sanity (host stub variant) ---------------------------
    void constructionDefaultStateIsIdle();
    void constructionFiresNoCallbacksBeforeAnyCommand();

    // ---- D3 URL mapping (toLocalUrl) ---------------------------------------
    void toLocalUrlMapsPlainPathToFileUrl();
    void toLocalUrlEmptyPathYieldsEmptyUrl();

    // ---- D3 URL mapping (toPlainPath) --------------------------------------
    void toPlainPathMapsFileUrlToPlainPath();
    void toPlainPathRejectsNonLocalUrls();

    // ---- D3 round trip: spaces / unusual bytes ------------------------------
    void urlRoundTripSurvivesSpacesAndPercentAndPunctuation();
    void urlRoundTripSurvivesNonAsciiUnicodeFilenames();

    // ---- SH-806: seek primitive (host stub variant) -------------------------
    void seekOnHostStubIsSafeAndFiresNoCallbacks();

    // ---- SH-903: gst error → SH-403 code mapping (pure helper) --------------
    void gstCoreMissingPluginMapsToSkippedUnsupported();
    void gstStreamCodecNotFoundMapsToSkippedUnsupported();
    void gstResourceErrorsMapToDecodeFailure();
    void gstStreamErrorsMapToDecodeFailure();
    void gstLibraryAndUnknownErrorsMapToEngineFailure();

private:
    static QString q(const std::string& s) { return QString::fromStdString(s); }
};

// ---- construction sanity ---------------------------------------------------

void TestEngineAdapter::constructionDefaultStateIsIdle() {
    ConstructorProbe probe;
    // Documented decision (SH-D402-2): a fresh adapter is Idle, has no
    // position, and reports an unknown duration; nothing was played.
    QCOMPARE(probe.adapter.state(), IAudioEngine::State::Idle);
    QCOMPARE(long(probe.adapter.positionSeconds()), 0L);
    QCOMPARE(long(probe.adapter.durationSeconds()),
             long(shanty::Track::kUnknownDuration));
}

void TestEngineAdapter::constructionFiresNoCallbacksBeforeAnyCommand() {
    ConstructorProbe probe;
    // Documented decision: no transport command → no callback. On the host
    // stub variant callbacks can never fire (transport no-ops are silent);
    // on the device variant construction must complete silently before any
    // command as well.
    QCOMPARE(probe.finishFired, 0);
    QCOMPARE(probe.errorFired, 0);
}

// ---- D3 URL mapping: plain path → file:// ----------------------------------

void TestEngineAdapter::toLocalUrlMapsPlainPathToFileUrl() {
    // D3: QUrl::fromLocalFile semantics, exactly.
    const QUrl url = AudioEngineAdapter::toLocalUrl("/music/track01.ogg");
    QCOMPARE(url, QUrl(QString::fromLatin1("file:///music/track01.ogg")));
    QVERIFY(url.isLocalFile());
}

void TestEngineAdapter::toLocalUrlEmptyPathYieldsEmptyUrl() {
    // Documented decision (SH-D402-3): nothing to play → empty QUrl, not
    // "file://".
    const QUrl url = AudioEngineAdapter::toLocalUrl(std::string());
    QVERIFY(url.isEmpty());
}

// ---- D3 URL mapping: file:// → plain path ----------------------------------

void TestEngineAdapter::toPlainPathMapsFileUrlToPlainPath() {
    const QUrl url = QUrl::fromLocalFile(QString::fromLatin1("/music/track01.ogg"));
    QCOMPARE(q(AudioEngineAdapter::toPlainPath(url)),
             QString::fromLatin1("/music/track01.ogg"));
}

void TestEngineAdapter::toPlainPathRejectsNonLocalUrls() {
    // Documented decision (SH-D402-3): non-local/empty URLs are unmappable
    // under D3 (local playback only) → "" (the documented "", not an
    // exception — callbacks, not exceptions).
    QCOMPARE(AudioEngineAdapter::toPlainPath(QUrl()), std::string());
    QCOMPARE(q(AudioEngineAdapter::toPlainPath(
                 QUrl(QString::fromLatin1("http://example.org/a.mp3")))),
             QString());
}

// ---- D3 round trip ---------------------------------------------------------

void TestEngineAdapter::urlRoundTripSurvivesSpacesAndPercentAndPunctuation() {
    // Spaces become %20 in the URL form; a literal '%' in a filename must be
    // re-encoded so toPlainPath does NOT double-decode it (fromLocalFile /
    // toLocalFile semantics).
    const std::string path = "/music/A Great Song %72+really, _fun_! (2016).flac";
    const QUrl url = AudioEngineAdapter::toLocalUrl(path);
    QCOMPARE(q(AudioEngineAdapter::toPlainPath(url)), QString::fromStdString(path));
}

void TestEngineAdapter::urlRoundTripSurvivesNonAsciiUnicodeFilenames() {
    const std::string paths[] = {
        "/musique/Café – 01.ogg",
        "/音乐/日本語 クラシック.flac",
        "/media/Königstiger_42.mp3",
    };
    for (const std::string& path : paths) {
        const QUrl url = AudioEngineAdapter::toLocalUrl(path);
        QCOMPARE(q(AudioEngineAdapter::toPlainPath(url)),
                 QString::fromStdString(path));
    }
}

void TestEngineAdapter::seekOnHostStubIsSafeAndFiresNoCallbacks() {
    ConstructorProbe probe;
    // SH-806 (SH-D402-2 host stub): nothing exists to move on the host —
    // the seek is a safe documented no-op: no crash, no callbacks, state
    // and position/duration observations unchanged.
    probe.adapter.seek(42);
    QCOMPARE(probe.finishFired, 0);
    QCOMPARE(probe.errorFired, 0);
    QCOMPARE(probe.adapter.state(), IAudioEngine::State::Idle);
    QCOMPARE(probe.adapter.positionSeconds(), long(0));
    QCOMPARE(probe.adapter.durationSeconds(),
             long(shanty::Track::kUnknownDuration));
}

// ---- SH-903: gst error → SH-403 code mapping (pure helper) ------------------
// The device engine moves to GStreamer (SH-E9, docs/eq-investigation.md).
// Bus ERROR messages carry (GError domain string, numeric code); the
// SH-403 supported-format gate matrix maps them to PlaybackError codes:
//   - "gst-core-error" MISSING_PLUGIN, "gst-stream-error" CODEC_NOT_FOUND
//     → SkippedUnsupported (the format gate surface);
//   - "gst-resource-error" (any), "gst-stream-error" (any other)
//     → DecodeFailure;
//   - "gst-library-error" and anything unknown → EngineFailure.
// Pure and gst-header-free on purpose: testable on the host.

void TestEngineAdapter::gstCoreMissingPluginMapsToSkippedUnsupported() {
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-core-error", 14),
             PlaybackError::Code::SkippedUnsupported);  // MISSING_PLUGIN
}

void TestEngineAdapter::gstStreamCodecNotFoundMapsToSkippedUnsupported() {
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-stream-error", 6),
             PlaybackError::Code::SkippedUnsupported);  // CODEC_NOT_FOUND
}

void TestEngineAdapter::gstResourceErrorsMapToDecodeFailure() {
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-resource-error", 3),
             PlaybackError::Code::DecodeFailure);  // NOT_FOUND
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-resource-error", 8),
             PlaybackError::Code::DecodeFailure);  // READ
}

void TestEngineAdapter::gstStreamErrorsMapToDecodeFailure() {
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-stream-error", 9),
             PlaybackError::Code::DecodeFailure);  // FORMAT
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-stream-error", 10),
             PlaybackError::Code::DecodeFailure);  // DECODE
}

void TestEngineAdapter::gstLibraryAndUnknownErrorsMapToEngineFailure() {
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-library-error", 1),
             PlaybackError::Code::EngineFailure);  // LIBRARY FAILED
    QCOMPARE(AudioEngineAdapter::mapGstError("gst-core-error", 1),
             PlaybackError::Code::EngineFailure);  // CORE FAILED
    QCOMPARE(AudioEngineAdapter::mapGstError("totally-unknown-domain", 1),
             PlaybackError::Code::EngineFailure);
}

QTEST_MAIN(TestEngineAdapter)
#include "tst_engine_adapter.moc"
