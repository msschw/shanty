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

QTEST_MAIN(TestEngineAdapter)
#include "tst_engine_adapter.moc"
