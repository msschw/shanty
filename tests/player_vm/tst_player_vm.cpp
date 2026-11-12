// SH-604 Phase A: PlayerViewModel logic behind the player view (no QML).
// Buttons → PlaybackController commands; progress mapping; seek clamp/
// rejection; queue-edge disabled states; fatal-error observation.
// Runs against the shared recording FakeAudioEngine (tests/fakes, reused
// read-only) and the landed PlaybackController facade — this suite locks the
// view-model decisions documented in presentation/viewmodels/PlayerViewModel.h.
// The view-model under test is compiled directly into this suite
// (SOURCES += …/PlayerViewModel.cpp; the epic owner folds it into
// presentation/viewmodels/viewmodels.pro later).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "fakes/FakeAudioEngine.h"
#include "playback/PlaybackController.h"
#include "playback/PlaybackQueue.h"
#include "ports/IAudioEngine.h"
#include "presentation/viewmodels/PlayerViewModel.h"

using shanty::PlaybackController;
using shanty::PlaybackError;
using shanty::PlaybackQueue;
using shanty::PlayerViewModel;

namespace {

/// Recording engine + queue + controller + view-model + observation logs,
/// fresh per test. The controller's own single-slot hooks stay UNTOUCHED
/// (documented view-model decision: the VM never assigns them).
struct Fixture {
    FakeAudioEngine engine;
    PlaybackQueue queue;
    PlaybackController controller{queue, engine};
    PlayerViewModel vm{controller, queue};

    std::vector<long> seekRequests;  ///< onSeekRequested log (clamped)
    int seekRejectedCalls = 0;       ///< onSeekRejected count
    int updatedCalls = 0;            ///< onUpdated count
    std::vector<std::string> rejections;  ///< onInvalidCommand log (SH-802)
    int playStartedCalls = 0;             ///< onPlayStarted count (SH-802)
    std::vector<std::string> confirmations;  ///< onConfirmed log (SH-803)

    Fixture() {
        vm.onSeekRequested = [this](long clamped) {
            seekRequests.push_back(clamped);
        };
        vm.onSeekRejected = [this]() { ++seekRejectedCalls; };
        vm.onUpdated = [this]() { ++updatedCalls; };
        vm.onInvalidCommand = [this](const char* reason) {
            rejections.push_back(reason ? reason : "(null)");
        };
        vm.onPlayStarted = [this]() { ++playStartedCalls; };
        vm.onConfirmed = [this](const char* message) {
            confirmations.push_back(message ? message : "(null)");
        };
    }

    /// Standard setup: three-entry queue loaded, not playing.
    void loadThreeEntries() { controller.loadQueue({"/a.mp3", "/b.ogg", "/c.flac"}); }
};

/// Absorb the double comparison into one helper (exact mapping, 1e-9).
bool fracNear(double actual, double expected) {
    return std::fabs(actual - expected) < 1e-9;
}

}  // namespace

class TestPlayerViewModel : public QObject {
    Q_OBJECT

private slots:
    // ---- buttons → controller commands -------------------------------------
    void playPauseButtonPlaysFromIdle();
    void playPauseButtonPausesThenResumes();
    void nextButtonForwardsAndPlaysNextEntry();
    void previousButtonForwardsAndPlaysPreviousEntry();

    // ---- progress mapping + refresh timing ---------------------------------
    void refreshIsTheSingleReReadEntryPoint();
    void progressFractionMapsPositionOverDuration();
    void progressFractionIsZeroWhenDurationMissingOrNonPositive();
    void progressFractionClampsToOneBeyondDuration();

    // ---- seek ----------------------------------------------------------------
    void seekToPassesClampedTargetToTheEngineHop();
    void seekToRejectsWithoutKnownDuration();

    // ---- derived queue-edge states ------------------------------------------
    void canGoPreviousFalseAtStartTrueAfterAdvancing();
    void canGoNextFalseAtLastEntryTrueBefore();
    void canGoFlagsFalseOnEmptyQueue();
    void queueObservablesExposedAfterJump();

    // ---- fatal-error observation + gate --------------------------------------
    void fatalErrorDisablesTransportAndSurfacesError();
    void skippedUnsupportedNeverDisablesTransport();
    void retryAfterFatalErrorReEnablesTransport();

    // ---- queue-replace play command (SH-802 / UX-2) ----------------------------
    void playFromPathsReplacesQueueSelectsFirstAndPlays();
    void playFromPathsSnapshotIsACopyOfTheGivenEntries();
    void playFromPathsRejectsEmptyPlaylistWithoutTouchingState();
    void playFromPathsFiresPlayStartedOnlyOnSuccess();

    // ---- confirmation channel (SH-803 / UX-7) ----------------------------------
    void playFromPathsConfirmsQueueBuiltVerbatim();
    void playFromPathsConfirmationSingularForm();
    void playFromPathsRejectionFiresNoConfirmation();
    void perTrackTransportChangesNeverConfirm();

    // ---- session restore snapshot (SH-805 / UX-4) -------------------------------
    void restoredSessionFollowsTheSnapshotRefreshContract();

    // ---- hooks ----------------------------------------------------------------
    void refreshFiresOnUpdatedExactlyOncePerCall();
};

// ---- buttons → controller commands -----------------------------------------

void TestPlayerViewModel::playPauseButtonPlaysFromIdle() {
    Fixture f;
    f.loadThreeEntries();

    // Documented togglePlayPause() mapping: Idle → play().
    f.vm.togglePlayPause();

    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
    QCOMPARE(QString::fromStdString(f.engine.playCalls.front()),
             QString::fromLatin1("/a.mp3"));
}

void TestPlayerViewModel::playPauseButtonPausesThenResumes() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    f.vm.togglePlayPause();  // Playing → pause
    QCOMPARE(f.controller.state(), PlaybackController::State::Paused);
    QCOMPARE(f.engine.pauseCalls, 1);

    f.vm.togglePlayPause();  // Paused → resume
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.engine.resumeCalls, 1);
    // No additional play issued while toggling pause/resume.
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
}

void TestPlayerViewModel::nextButtonForwardsAndPlaysNextEntry() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    f.vm.next();

    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.engine.playCalls.size(), size_t(2));
    QCOMPARE(QString::fromStdString(f.engine.playCalls.back()),
             QString::fromLatin1("/b.ogg"));
}

void TestPlayerViewModel::previousButtonForwardsAndPlaysPreviousEntry() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.jumpTo(2);  // park on the last entry

    f.vm.previous();

    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(QString::fromStdString(f.engine.playCalls.back()),
             QString::fromLatin1("/b.ogg"));
}

// ---- progress mapping + refresh timing --------------------------------------

void TestPlayerViewModel::refreshIsTheSingleReReadEntryPoint() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    f.engine.posSeconds = 30;
    f.engine.durSeconds = 100;
    f.vm.refresh();

    QCOMPARE(f.vm.positionSeconds(), 30L);
    QCOMPARE(f.vm.durationSeconds(), 100L);
    QVERIFY(fracNear(f.vm.progressFraction(), 0.3));

    // The engine moves on; the snapshot stays stale ON PURPOSE (documented
    // decision: re-read happens only in refresh(), called by the UI timer
    // and after controller state changes) until the next refresh().
    f.engine.posSeconds = 45;
    QCOMPARE(f.vm.positionSeconds(), 30L);
    QVERIFY(fracNear(f.vm.progressFraction(), 0.3));

    f.vm.refresh();
    QCOMPARE(f.vm.positionSeconds(), 45L);
    QVERIFY(fracNear(f.vm.progressFraction(), 0.45));
}

void TestPlayerViewModel::progressFractionMapsPositionOverDuration() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    f.engine.posSeconds = 0;
    f.engine.durSeconds = 240;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 0.0));  // start → 0.0

    f.engine.posSeconds = 60;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 0.25));

    f.engine.posSeconds = 240;  // position == duration → 1.0
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 1.0));
}

void TestPlayerViewModel::progressFractionIsZeroWhenDurationMissingOrNonPositive() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    // Documented rule: unknown duration (Track::kUnknownDuration = -1) → 0.0.
    f.engine.posSeconds = 12;
    f.engine.durSeconds = shanty::Track::kUnknownDuration;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 0.0));

    // Documented rule: 0 → 0.0, negative → 0.0 (no known span).
    f.engine.durSeconds = 0;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 0.0));
    f.engine.durSeconds = -5;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 0.0));

    // Documented rule: negative position clamps to 0.0 even with a duration.
    f.engine.posSeconds = -1;
    f.engine.durSeconds = 100;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 0.0));
}

void TestPlayerViewModel::progressFractionClampsToOneBeyondDuration() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    // Documented rule: position beyond duration never exceeds 1.0.
    f.engine.posSeconds = 150;
    f.engine.durSeconds = 100;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 1.0));

    f.engine.posSeconds = 10000;
    f.vm.refresh();
    QVERIFY(fracNear(f.vm.progressFraction(), 1.0));
}

// ---- seek ----------------------------------------------------------------

void TestPlayerViewModel::seekToPassesClampedTargetToTheEngineHop() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    f.engine.durSeconds = 200;
    f.vm.refresh();

    // In-range target passes through unchanged — this IS the asserted
    // argument handed to the application layer's engine hop (deviation note
    // in PlayerViewModel.h: the SH-402 read-only port has no seek primitive
    // yet, so the clamped value is surfaced via onSeekRequested).
    f.vm.seekTo(97);
    QCOMPARE(f.seekRequests.size(), size_t(1));
    QCOMPARE(f.seekRequests.back(), 97L);
    QCOMPARE(f.seekRejectedCalls, 0);

    // Documented clamp rules on the SAME snapshot duration:
    f.vm.seekTo(-7);   // negative → 0
    f.vm.seekTo(999);  // beyond duration → duration
    QCOMPARE(f.seekRequests.size(), size_t(3));
    QCOMPARE(f.seekRequests[1], 0L);
    QCOMPARE(f.seekRequests[2], 200L);
    QCOMPARE(f.seekRejectedCalls, 0);
}

void TestPlayerViewModel::seekToRejectsWithoutKnownDuration() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();

    // Unknown duration → no clamp anchor → rejected (documented decision).
    f.engine.durSeconds = shanty::Track::kUnknownDuration;
    f.vm.refresh();
    f.vm.seekTo(15);
    QCOMPARE(f.seekRequests.size(), size_t(0));
    QCOMPARE(f.seekRejectedCalls, 1);

    // Same rejection for a 0-duration snapshot.
    f.engine.durSeconds = 0;
    f.vm.refresh();
    f.vm.seekTo(15);
    QCOMPARE(f.seekRequests.size(), size_t(0));
    QCOMPARE(f.seekRejectedCalls, 2);
}

// ---- derived queue-edge states ----------------------------------------------

void TestPlayerViewModel::canGoPreviousFalseAtStartTrueAfterAdvancing() {
    Fixture f;
    f.loadThreeEntries();
    f.vm.refresh();  // snapshot at index 0

    // Documented rule: canGoPrevious = false at index 0 (no wrap).
    QCOMPARE(f.vm.canGoPrevious(), false);
    QCOMPARE(f.vm.canGoNext(), true);

    f.controller.next();
    f.vm.refresh();
    QCOMPARE(f.vm.canGoPrevious(), true);
    QCOMPARE(f.vm.canGoNext(), true);
}

void TestPlayerViewModel::canGoNextFalseAtLastEntryTrueBefore() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.jumpTo(2);  // last of 3
    f.vm.refresh();

    // Documented rule: canGoNext computed against REMAINING entries —
    // false on the last entry (stop-at-end, no wrap).
    QCOMPARE(f.vm.canGoNext(), false);
    QCOMPARE(f.vm.canGoPrevious(), true);

    f.controller.previous();
    f.vm.refresh();
    QCOMPARE(f.vm.canGoNext(), true);
}

void TestPlayerViewModel::canGoFlagsFalseOnEmptyQueue() {
    Fixture f;
    f.vm.refresh();  // no queue loaded

    // Documented rule: both false on an empty queue.
    QCOMPARE(f.vm.canGoNext(), false);
    QCOMPARE(f.vm.canGoPrevious(), false);
    QCOMPARE(f.vm.queueCount(), size_t(0));
}

void TestPlayerViewModel::queueObservablesExposedAfterJump() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.jumpTo(1);
    f.vm.refresh();

    QCOMPARE(f.vm.currentEntry(), std::string("/b.ogg"));
    QCOMPARE(f.vm.currentIndex(), size_t(1));
    QCOMPARE(f.vm.queueCount(), size_t(3));
    QCOMPARE(f.vm.state(), PlaybackController::State::Playing);
}

// ---- fatal-error observation + gate ------------------------------------------

void TestPlayerViewModel::fatalErrorDisablesTransportAndSurfacesError() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();
    f.vm.refresh();
    QCOMPARE(f.vm.transportEnabled(), true);
    QCOMPARE(f.vm.hasError(), false);

    // DecodeFailure is FATAL per the SH-403 matrix: recorded + Stopped.
    f.engine.emitError(PlaybackError::Code::DecodeFailure, "/a.mp3",
                       "decoder choked on the stream");
    f.vm.refresh();

    // Documented gate: fatal stop state disables the transport buttons.
    QCOMPARE(f.vm.transportEnabled(), false);
    QCOMPARE(f.vm.state(), PlaybackController::State::Stopped);

    // Error observation surfaces code + message + failing path for the panel.
    QCOMPARE(f.vm.hasError(), true);
    QCOMPARE(f.vm.lastErrorCode(), PlaybackError::Code::DecodeFailure);
    QCOMPARE(f.vm.lastErrorMessage(), std::string("decoder choked on the stream"));
    QCOMPARE(f.vm.lastErrorPath(), std::string("/a.mp3"));
}

void TestPlayerViewModel::skippedUnsupportedNeverDisablesTransport() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();
    f.vm.refresh();

    // SkippedUnsupported mid-queue: skip + advance, still Playing (SH-404).
    f.engine.emitError(PlaybackError::Code::SkippedUnsupported, "/a.mp3",
                       "unsupported wav container");
    f.vm.refresh();

    QCOMPARE(f.vm.state(), PlaybackController::State::Playing);
    QCOMPARE(f.vm.currentIndex(), size_t(1));  // advanced to /b.ogg
    QCOMPARE(f.vm.transportEnabled(), true);   // never a fatal gate
    QCOMPARE(f.vm.hasError(), true);
    QCOMPARE(f.vm.lastErrorCode(), PlaybackError::Code::SkippedUnsupported);
    QCOMPARE(f.vm.lastErrorMessage(), std::string("unsupported wav container"));

    // Even when the skip stops at the LAST entry (Stopped + error recorded),
    // the gate matches the CODE, not the stopped state → still enabled.
    Fixture g;
    g.controller.loadQueue({"/only_entry.mid"});
    g.controller.play();
    g.engine.emitError(PlaybackError::Code::SkippedUnsupported, "/last.ogg",
                       "unsupported");
    g.vm.refresh();
    QCOMPARE(g.vm.state(), PlaybackController::State::Stopped);
    QCOMPARE(g.vm.transportEnabled(), true);
}

void TestPlayerViewModel::retryAfterFatalErrorReEnablesTransport() {
    Fixture f;
    f.loadThreeEntries();
    f.controller.play();
    f.engine.emitError(PlaybackError::Code::EngineFailure, "/a.mp3",
                       "sink went away");
    f.vm.refresh();
    QCOMPARE(f.vm.transportEnabled(), false);

    // Documented recovery: explicit fresh attempt (SH-404 semantics —
    // clears the recorded error, replays the selected entry).
    f.vm.retryPlayback();
    f.vm.refresh();

    QCOMPARE(f.vm.transportEnabled(), true);
    QCOMPARE(f.vm.hasError(), false);
    QCOMPARE(f.vm.state(), PlaybackController::State::Playing);
    QCOMPARE(f.vm.lastErrorMessage(), std::string());  // panel cleared
    QCOMPARE(f.engine.playCalls.size(), size_t(2));     // replayed /a.mp3
    QCOMPARE(QString::fromStdString(f.engine.playCalls.back()),
             QString::fromLatin1("/a.mp3"));
}

// ---- hooks ---------------------------------------------------------------------

void TestPlayerViewModel::refreshFiresOnUpdatedExactlyOncePerCall() {
    Fixture f;
    f.loadThreeEntries();

    QCOMPARE(f.updatedCalls, 0);
    f.vm.refresh();
    QCOMPARE(f.updatedCalls, 1);
    f.vm.refresh();
    f.vm.refresh();
    QCOMPARE(f.updatedCalls, 3);
}

// ---- queue-replace play command (SH-802 / UX-2) ---------------------------------

void TestPlayerViewModel::playFromPathsReplacesQueueSelectsFirstAndPlays() {
    Fixture f;
    f.vm.playFromPaths({"/p1.mp3", "/p2.ogg", "/p3.flac"});
    f.vm.refresh();  // snapshot contract: getters follow the last refresh

    // Playlist order → queue order; selection → index 0; playing.
    QCOMPARE(f.vm.queueCount(), size_t(3));
    QCOMPARE(f.vm.currentIndex(), size_t(0));
    QCOMPARE(f.vm.state(), PlaybackController::State::Playing);
    QCOMPARE(f.vm.currentEntry(), std::string("/p1.mp3"));
    QCOMPARE(f.rejections.size(), size_t(0));
}

void TestPlayerViewModel::playFromPathsSnapshotIsACopyOfTheGivenEntries() {
    Fixture f;
    std::vector<std::string> entries{"/s1.mp3", "/s2.ogg"};
    f.vm.playFromPaths(entries);
    f.vm.refresh();
    QCOMPARE(f.vm.queueCount(), size_t(2));

    // Queue-snapshot semantics (UX-2): mutating the caller's list after the
    // command never mutates the running queue.
    entries.clear();
    entries.push_back("/unrelated.mp3");
    f.vm.refresh();
    QCOMPARE(f.vm.queueCount(), size_t(2));
    QCOMPARE(f.vm.currentEntry(), std::string("/s1.mp3"));
}

void TestPlayerViewModel::playFromPathsRejectsEmptyPlaylistWithoutTouchingState() {
    Fixture f;
    // Build a live session first: queue of 3, index 1, playing.
    f.loadThreeEntries();
    f.controller.jumpTo(1);
    f.vm.refresh();
    QCOMPARE(f.vm.state(), PlaybackController::State::Playing);
    QCOMPARE(f.vm.currentIndex(), size_t(1));

    f.vm.playFromPaths({});

    // Documented rejection with the stable catalogue string...
    QCOMPARE(f.rejections.size(), size_t(1));
    QCOMPARE(QString::fromStdString(f.rejections.back()),
             QString("play: playlist is empty"));
    // ...and NO state change: queue, selection and transport untouched.
    f.vm.refresh();
    QCOMPARE(f.vm.queueCount(), size_t(3));
    QCOMPARE(f.vm.currentIndex(), size_t(1));
    QCOMPARE(f.vm.state(), PlaybackController::State::Playing);
    QCOMPARE(f.playStartedCalls, 0);
}

void TestPlayerViewModel::playFromPathsFiresPlayStartedOnlyOnSuccess() {
    Fixture f;
    f.vm.playFromPaths({"/q1.mp3"});
    QCOMPARE(f.playStartedCalls, 1);
    QCOMPARE(f.rejections.size(), size_t(0));

    // The rejection path must NOT fire onPlayStarted.
    f.vm.playFromPaths({});
    QCOMPARE(f.playStartedCalls, 1);
    QCOMPARE(f.rejections.size(), size_t(1));
}

// ---- confirmation channel (SH-803 / UX-7) ---------------------------------------

void TestPlayerViewModel::playFromPathsConfirmsQueueBuiltVerbatim() {
    Fixture f;
    f.vm.playFromPaths({"/c1.mp3", "/c2.ogg", "/c3.flac"});

    // UX-7: the queue-building action confirms with the verbatim catalogue
    // string, count = queue size after the replace.
    QCOMPARE(f.confirmations.size(), size_t(1));
    QCOMPARE(QString::fromStdString(f.confirmations.back()),
             QString("queue: built from playlist (3 entries)"));
    // Success path unchanged: playStarted still fires, no rejection.
    QCOMPARE(f.playStartedCalls, 1);
    QCOMPARE(f.rejections.size(), size_t(0));
}

void TestPlayerViewModel::playFromPathsConfirmationSingularForm() {
    Fixture f;
    f.vm.playFromPaths({"/one.mp3"});
    QCOMPARE(f.confirmations.size(), size_t(1));
    QCOMPARE(QString::fromStdString(f.confirmations.back()),
             QString("queue: built from playlist (1 entry)"));
}

void TestPlayerViewModel::playFromPathsRejectionFiresNoConfirmation() {
    Fixture f;
    f.vm.playFromPaths({});
    // Sparse rule: the rejection path confirms NOTHING.
    QCOMPARE(f.confirmations.size(), size_t(0));
    QCOMPARE(f.rejections.size(), size_t(1));
}

void TestPlayerViewModel::perTrackTransportChangesNeverConfirm() {
    Fixture f;
    f.loadThreeEntries();
    QCOMPARE(f.confirmations.size(), size_t(0));

    // Per-track changes (SH-604 transport commands on an existing queue)
    // never confirm — the Player page/cover already surface them (UX-7).
    f.vm.togglePlayPause();
    f.vm.togglePlayPause();
    f.vm.next();
    f.vm.previous();
    f.controller.jumpTo(2);
    f.vm.refresh();
    QCOMPARE(f.confirmations.size(), size_t(0));
}

void TestPlayerViewModel::restoredSessionFollowsTheSnapshotRefreshContract() {
    Fixture f;
    f.controller.restoreQueue({"/r1.mp3", "/r2.ogg"}, 1, 30);

    // Snapshot contract (SH-805 device finding, startup landing): the
    // getters are INTENTIONALLY stale until the documented single re-read —
    // the app wiring must drive refresh() ONCE after the restore and before
    // any snapshot getter is read for the landing decision (SH-801), the
    // cover data or the first 1 Hz persistence save.
    QCOMPARE(f.vm.queueCount(), size_t(0));
    QCOMPARE(f.vm.state(), PlaybackController::State::Idle);
    QCOMPARE(f.vm.positionSeconds(), long(0));

    f.vm.refresh();
    QCOMPARE(f.vm.queueCount(), size_t(2));
    QCOMPARE(f.vm.currentIndex(), size_t(1));
    QCOMPARE(f.vm.currentEntry(), std::string("/r2.ogg"));
    QCOMPARE(f.vm.state(), PlaybackController::State::Paused);
    QCOMPARE(f.vm.positionSeconds(), long(30));
}

QTEST_MAIN(TestPlayerViewModel)
#include "tst_player_vm.moc"
