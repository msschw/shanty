// SH-404 Phase A: PlaybackController facade semantics (D2) + SH-403 error
// fallback matrix, exercised against the recording FakeAudioEngine (the fake
// is trusted; the controller under test is what this suite drives).
// Phase A runs against the SH-404 stubs: every target case must fail by
// ASSERTION (QFATAL count 0), never by crash.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <cstddef>
#include <string>
#include <vector>

#include "playback/PlaybackController.h"
#include "fakes/FakeAudioEngine.h"

using shanty::PlaybackController;
using shanty::PlaybackError;
using shanty::PlaybackQueue;

namespace {

/// Fresh recording engine + queue + controller + observation log per test.
struct Fixture {
    FakeAudioEngine engine;
    PlaybackQueue queue;
    PlaybackController controller{queue, engine};

    std::vector<std::string> invalidReasons;  ///< onInvalidCommand log
    std::vector<PlaybackController::State> transitions;  ///< onStateChanged log

    Fixture() {
        controller.onInvalidCommand = [this](const char* reason) {
            invalidReasons.push_back(reason ? reason : "(null)");
        };
        controller.onStateChanged = [this]() {
            transitions.push_back(controller.state());
        };
    }
};

QString asQ(const std::string& s) { return QString::fromStdString(s); }

/// Last logged invalid-command reason (helper keeps the side agnostic).
QString lastReason(const std::vector<std::string>& reasons) {
    return reasons.empty() ? QString() : asQ(reasons.back());
}

}  // namespace

class TestPlaybackController : public QObject {
    Q_OBJECT

private slots:
    // ---- load + transport (SH-404 state matrix) ---------------------------
    void loadQueueSelectsFirstEntryAndStaysIdle();
    void loadQueueWhilePlayingStopsEngine();
    void playIssuesEnginePlayWithCurrentPathAndEntersPlaying();
    void togglePlayPauseCyclesPauseAndResume();
    void stopTransitionsToStoppedAndStopsEngine();

    // ---- end of track (SH-401 stop-at-end through the controller) ---------
    void trackFinishedMidQueueAdvancesAndPlaysNext();
    void trackFinishedAtLastEntryStopsWithoutWrapping();

    // ---- SH-403 error fallback --------------------------------------------
    void skipUnsupportedErrorAdvancesAndPlaysNext();
    void skipUnsupportedErrorAtLastEntryStops();
    void decodeFailureErrorStopsRecordsErrorAndDoesNotAutoSkip();
    void errorIsClearedByNextPlay();

    // ---- transport commands + invalid-command hook -------------------------
    void nextAdvancesAndPlaysNextEntry();
    void previousRetreatsAndPlaysPreviousEntry();
    void nextBeyondQueueIsInvalidNoop();
    void emptyQueueCommandsAreInvalidNoops();
    void jumpToSelectsAndPlaysTargetEntry();
    void jumpToOutOfRangeIsInvalidNoop();

    // ---- session restore (SH-805 / UX-4, restore-paused invariant) ---------
    void restoreQueueSelectsIndexPausedWithoutEngineCalls();
    void restoredSessionReportsRestoredPositionUntilFirstPlay();
    void restoredSessionResumeStartsAudioThroughEnginePlay();
    void restoredSessionAnyPlayDropsTheRestoredPosition();
    void restoreQueueEmptyPathsIsRejectedNoop();
    void restoreQueueIndexClampedToLast();

    // ---- seek (SH-806 / UX-10, SH-D604-B1 no-op dies) -----------------------
    void seekWhilePlayingIssuesEngineSeekAndKeepsState();
    void seekWhilePausedIssuesEngineSeekAndKeepsState();
    void seekWhileStoppedIsDocumentedNoop();
    void seekWhileIdleIsDocumentedNoop();
    void seekOnFreshRestoredSessionDropsTheRestoredOffset();

    // ---- engine passthrough ------------------------------------------------
    void positionAndDurationPassThroughFromEngine();
};

void TestPlaybackController::loadQueueSelectsFirstEntryAndStaysIdle() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg", "/c.flac"});

    QCOMPARE(asQ(f.controller.currentEntry()), QString::fromLatin1("/a.mp3"));
    QCOMPARE(f.controller.currentIndex(), size_t(0));
    // Documented decision: a loaded queue starts stopped, NOT auto-playing.
    QCOMPARE(f.controller.state(), PlaybackController::State::Idle);
    // No engine command and no recorded error before play().
    QCOMPARE(f.engine.playCalls.size(), size_t(0));
    QCOMPARE(f.engine.stopCalls, 0);
    QCOMPARE(f.controller.hasError(), false);
}

void TestPlaybackController::loadQueueWhilePlayingStopsEngine() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.controller.loadQueue({"/c.flac"});

    // Documented decision: replacing the queue of a playing engine stops it.
    QCOMPARE(f.engine.stopCalls, 1);
    QCOMPARE(f.controller.state(), PlaybackController::State::Idle);
    // Selection resets to the first entry of the new queue.
    QCOMPARE(asQ(f.controller.currentEntry()), QString::fromLatin1("/c.flac"));
    QCOMPARE(f.controller.currentIndex(), size_t(0));
}

void TestPlaybackController::playIssuesEnginePlayWithCurrentPathAndEntersPlaying() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();

    // D2: the engine receives exactly "play this path now".
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
    QCOMPARE(asQ(f.engine.playCalls.front()), QString::fromLatin1("/a.mp3"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);

    // Observable transition Idle → Playing was exposed to observers.
    QCOMPARE(f.transitions.size(), size_t(1));
    QCOMPARE(f.transitions.front(), PlaybackController::State::Playing);

    // play() never issues pause/resume/stop on the happy path.
    QCOMPARE(f.engine.pauseCalls, 0);
    QCOMPARE(f.engine.resumeCalls, 0);
    QCOMPARE(f.engine.stopCalls, 0);
}

void TestPlaybackController::togglePlayPauseCyclesPauseAndResume() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();

    f.controller.togglePlayPause();  // Playing → pause
    QCOMPARE(f.engine.pauseCalls, 1);
    QCOMPARE(f.controller.state(), PlaybackController::State::Paused);

    f.controller.togglePlayPause();  // Paused → resume
    QCOMPARE(f.engine.resumeCalls, 1);
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);

    // Toggling on an empty queue is a documented invalid no-op.
    Fixture empty;
    empty.controller.togglePlayPause();
    QCOMPARE(empty.engine.playCalls.size(), size_t(0));
    QCOMPARE(empty.engine.pauseCalls, 0);
    QCOMPARE(empty.engine.resumeCalls, 0);
    QCOMPARE(empty.controller.state(), PlaybackController::State::Idle);
    QCOMPARE(empty.invalidReasons.size(), size_t(1));
}

void TestPlaybackController::stopTransitionsToStoppedAndStopsEngine() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.controller.stop();

    QCOMPARE(f.engine.stopCalls, 1);
    QCOMPARE(f.controller.state(), PlaybackController::State::Stopped);
    // The queue selection survives a stop (documented: Stopped, not Idle).
    QCOMPARE(asQ(f.controller.currentEntry()), QString::fromLatin1("/a.mp3"));

    // stop() from Idle is a documented invalid no-op (nothing to stop).
    Fixture idle;
    idle.controller.stop();
    QCOMPARE(idle.engine.stopCalls, 0);
    QCOMPARE(idle.controller.state(), PlaybackController::State::Idle);
    QCOMPARE(idle.invalidReasons.size(), size_t(1));
}

void TestPlaybackController::trackFinishedMidQueueAdvancesAndPlaysNext() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg", "/c.flac"});
    f.controller.play();
    f.engine.finishTrack();  // engine reports the natural end of "/a.mp3"

    // SH-401 queue semantics drove the advance; D2: engine got play(next).
    QCOMPARE(f.engine.playCalls.size(), size_t(2));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/b.ogg"));
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    // Gapless advance: no stop needed between the two entries.
    QCOMPARE(f.engine.stopCalls, 0);
}

void TestPlaybackController::trackFinishedAtLastEntryStopsWithoutWrapping() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.engine.finishTrack();  // natural end of "/a.mp3" → plays "/b.ogg"
    f.engine.finishTrack();  // natural end of the LAST entry

    // SH-401 stop-at-end: no wrap-around, the controller reports stopped.
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.controller.state(), PlaybackController::State::Stopped);
    QCOMPARE(f.engine.stopCalls, 1);
    // Only two play()s ever happened ("/a", then "/b"); no wrap replay.
    QCOMPARE(f.engine.playCalls.size(), size_t(2));
}

void TestPlaybackController::skipUnsupportedErrorAdvancesAndPlaysNext() {
    Fixture f;
    f.controller.loadQueue({"/a.wav", "/b.ogg", "/c.flac"});
    f.controller.play();
    f.engine.emitError(PlaybackError::Code::SkippedUnsupported, "/a.wav",
                       "wav is not a supported audio format");

    // SH-403: unsupported files are a controlled skip, not a stop/crash.
    QCOMPARE(f.engine.playCalls.size(), size_t(2));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/b.ogg"));
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.engine.stopCalls, 0);
    // The skip is recorded for observation (SH-403 error entry).
    QCOMPARE(f.controller.hasError(), true);
    QCOMPARE(f.controller.lastError().code,
             PlaybackError::Code::SkippedUnsupported);
    QCOMPARE(asQ(f.controller.lastError().path), QString::fromLatin1("/a.wav"));
}

void TestPlaybackController::skipUnsupportedErrorAtLastEntryStops() {
    Fixture f;
    f.controller.loadQueue({"/a.ogg", "/b.wav"});
    f.controller.jumpTo(1);  // "/b.wav" is playing (jumpTo issues the play)
    f.engine.emitError(PlaybackError::Code::SkippedUnsupported, "/b.wav",
                       "wav is not a supported audio format");

    // Stop-at-end applies to skips too: nothing to advance onto → stop.
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.controller.state(), PlaybackController::State::Stopped);
    QCOMPARE(f.engine.stopCalls, 1);
    // No wrap-around replay.
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
}

void TestPlaybackController::decodeFailureErrorStopsRecordsErrorAndDoesNotAutoSkip() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.engine.emitError(PlaybackError::Code::DecodeFailure, "/a.mp3",
                       "decoder gave no valid stream");

    // Documented decision: decode failures are FATAL — no auto-skip.
    QCOMPARE(f.controller.state(), PlaybackController::State::Stopped);
    QCOMPARE(f.engine.stopCalls, 1);
    // The queue selection does NOT advance and no play(next) was issued.
    QCOMPARE(f.controller.currentIndex(), size_t(0));
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
    // The last error is recorded for observation.
    QCOMPARE(f.controller.hasError(), true);
    QCOMPARE(f.controller.lastError().code, PlaybackError::Code::DecodeFailure);
    QCOMPARE(asQ(f.controller.lastError().path), QString::fromLatin1("/a.mp3"));
    QCOMPARE(asQ(f.controller.lastError().message),
             QString::fromLatin1("decoder gave no valid stream"));
}

void TestPlaybackController::errorIsClearedByNextPlay() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.engine.emitError(PlaybackError::Code::DecodeFailure, "/a.mp3", "boom");
    QCOMPARE(f.controller.hasError(), true);

    // Documented decision: a fresh explicit play() is a fresh attempt and
    // drops the recorded error (the entry is replayed, not skipped).
    f.controller.play();
    QCOMPARE(f.controller.hasError(), false);
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    // Same entry replayed, still no advance.
    QCOMPARE(f.engine.playCalls.size(), size_t(2));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/a.mp3"));
}

void TestPlaybackController::nextAdvancesAndPlaysNextEntry() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg", "/c.flac"});
    f.controller.play();
    f.controller.next();

    // Transport semantics: the target entry is selected AND played.
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/b.ogg"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);

    f.controller.next();
    QCOMPARE(f.controller.currentIndex(), size_t(2));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/c.flac"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    // Each advance reached the engine as a play command (no pause/stop churn).
    QCOMPARE(f.engine.playCalls.size(), size_t(3));
    QCOMPARE(f.engine.stopCalls, 0);
}

void TestPlaybackController::previousRetreatsAndPlaysPreviousEntry() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg", "/c.flac"});
    f.controller.jumpTo(2);  // "/c.flac"
    QCOMPARE(f.controller.currentIndex(), size_t(2));

    f.controller.previous();
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/b.ogg"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);

    // previous() at the first entry is a documented invalid no-op
    // (SH-401: never a negative position).
    f.controller.previous();  // "/b" → "/a"
    f.controller.previous();  // already at "/a" → no-op
    QCOMPARE(f.controller.currentIndex(), size_t(0));
    QCOMPARE(f.engine.playCalls.size(), size_t(3));  // "/c", "/b", "/a"
    QCOMPARE(lastReason(f.invalidReasons), QString("previous: start of queue"));
}

void TestPlaybackController::nextBeyondQueueIsInvalidNoop() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.controller.next();  // valid advance onto the last entry
    const size_t playCount = f.engine.playCalls.size();
    QCOMPARE(playCount, size_t(2));

    f.controller.next();  // beyond the LAST entry (stop-at-end)
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(f.engine.playCalls.size(), playCount);  // no new engine command
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("next: end of queue"));
}

void TestPlaybackController::emptyQueueCommandsAreInvalidNoops() {
    Fixture f;
    // next / previous / jumpTo / play on an empty queue: every one of them is
    // a documented no-op exposed via the invalid-command hook.
    f.controller.next();
    f.controller.previous();
    QCOMPARE(f.controller.jumpTo(0), false);
    f.controller.play();
    f.controller.togglePlayPause();  // routes to play → also invalid

    QCOMPARE(f.invalidReasons.size(), size_t(5));
    QCOMPARE(lastReason(f.invalidReasons), QString("play: empty queue"));
    QCOMPARE(f.engine.playCalls.size(), size_t(0));
    QCOMPARE(f.engine.stopCalls, 0);
    QCOMPARE(f.controller.state(), PlaybackController::State::Idle);
    // jumpTo stayed a no-op (returned false, selection untouched) and play
    // left nothing recorded.
    QCOMPARE(f.controller.currentIndex(), size_t(0));
    QCOMPARE(asQ(f.controller.currentEntry()), QString());
    QCOMPARE(f.controller.hasError(), false);
}

void TestPlaybackController::jumpToSelectsAndPlaysTargetEntry() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg", "/c.flac"});
    const bool accepted = f.controller.jumpTo(2);

    QCOMPARE(accepted, true);
    QCOMPARE(f.controller.currentIndex(), size_t(2));
    QCOMPARE(asQ(f.controller.currentEntry()), QString::fromLatin1("/c.flac"));
    // Documented decision: jumpTo starts playing the target entry.
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
    QCOMPARE(asQ(f.engine.playCalls.back()), QString::fromLatin1("/c.flac"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
}

void TestPlaybackController::jumpToOutOfRangeIsInvalidNoop() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    const bool accepted = f.controller.jumpTo(2);  // size 2 → out of range

    QCOMPARE(accepted, false);
    QCOMPARE(f.controller.currentIndex(), size_t(0));
    QCOMPARE(f.controller.state(), PlaybackController::State::Idle);
    QCOMPARE(f.engine.playCalls.size(), size_t(0));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("jumpTo: index out of range"));
}

void TestPlaybackController::positionAndDurationPassThroughFromEngine() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();

    // Unknown duration before the engine reports one.
    QCOMPARE(f.controller.positionSeconds(), long(0));
    QCOMPARE(f.controller.durationSeconds(),
             long(shanty::Track::kUnknownDuration));

    // Pure passthrough from the engine port.
    f.engine.posSeconds = 42;
    f.engine.durSeconds = 187;
    QCOMPARE(f.controller.positionSeconds(), long(42));
    QCOMPARE(f.controller.durationSeconds(), long(187));
}

// ---- session restore (SH-805 / UX-4, restore-paused invariant) -----------------

void TestPlaybackController::restoreQueueSelectsIndexPausedWithoutEngineCalls() {
    Fixture f;
    f.controller.restoreQueue({"/a.mp3", "/b.ogg", "/c.flac"}, 1, 42);

    // Restore-paused invariant: restoring NEVER starts (or touches) audio.
    QCOMPARE(f.engine.playCalls.size(), size_t(0));
    QCOMPARE(f.engine.pauseCalls, 0);
    QCOMPARE(f.engine.resumeCalls, 0);
    QCOMPARE(f.engine.stopCalls, 0);
    // The queue is adopted in the given order at the clamped saved index...
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(asQ(f.controller.currentEntry()), QString::fromLatin1("/b.ogg"));
    // ...and the transport sits PAUSED (never Playing, never auto-started).
    QCOMPARE(f.controller.state(), PlaybackController::State::Paused);
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestPlaybackController::restoredSessionReportsRestoredPositionUntilFirstPlay() {
    Fixture f;
    f.controller.restoreQueue({"/a.mp3", "/b.ogg"}, 0, 95);
    // The engine has not played; the display shows the persisted position.
    f.engine.posSeconds = 7;  // the engine's own passthrough is ignored while fresh
    QCOMPARE(f.controller.positionSeconds(), long(95));
}

void TestPlaybackController::restoredSessionResumeStartsAudioThroughEnginePlay() {
    Fixture f;
    f.controller.restoreQueue({"/a.mp3", "/b.ogg"}, 0, 95);

    // A paused restored session must become audible on the user's resume —
    // the engine was never started, so resume() issues engine.play (the
    // fresh-attempt path), not engine.resume().
    f.controller.resume();
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
    QCOMPARE(asQ(f.engine.playCalls.front()), QString::fromLatin1("/a.mp3"));
    QCOMPARE(f.engine.resumeCalls, 0);
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);

    // The restored offset is dropped: the position is engine passthrough
    // again (the fresh-attempt play restarts the engine at its own position).
    f.engine.posSeconds = 3;
    QCOMPARE(f.controller.positionSeconds(), long(3));
}

void TestPlaybackController::restoredSessionAnyPlayDropsTheRestoredPosition() {
    Fixture f;
    f.controller.restoreQueue({"/a.mp3", "/b.ogg", "/c.flac"}, 0, 95);

    // Any user-initiated play (here: next()) drops the restored offset.
    f.controller.next();
    QCOMPARE(f.engine.playCalls.size(), size_t(1));
    f.engine.posSeconds = 2;
    QCOMPARE(f.controller.positionSeconds(), long(2));

    // And a fresh restore after a play is again display-only until played.
    f.controller.restoreQueue({"/a.mp3"}, 0, 60);
    f.engine.posSeconds = 1;
    QCOMPARE(f.controller.positionSeconds(), long(60));
}

void TestPlaybackController::restoreQueueEmptyPathsIsRejectedNoop() {
    Fixture f;
    // Build a live session first: queue + playing.
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.controller.restoreQueue({}, 0, 10);

    // Documented rejection; queue, selection and transport untouched.
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("restore: empty queue"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.controller.currentIndex(), size_t(0));
    QCOMPARE(f.controller.positionSeconds(), long(0));
}

void TestPlaybackController::restoreQueueIndexClampedToLast() {
    Fixture f;
    f.controller.restoreQueue({"/a.mp3", "/b.ogg"}, 99, 5);
    // Selection clamps into the restored queue (documented setCurrent rule).
    QCOMPARE(f.controller.currentIndex(), size_t(1));
    QCOMPARE(asQ(f.controller.currentEntry()), QString::fromLatin1("/b.ogg"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Paused);
}

// ---- seek (SH-806 / UX-10, SH-D604-B1 no-op dies) -------------------------------

void TestPlaybackController::seekWhilePlayingIssuesEngineSeekAndKeepsState() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.controller.seek(30);

    // The command reaches the engine as an absolute jump; the transport
    // state is unchanged (seek is not a play/pause/stop).
    QCOMPARE(f.engine.seekCalls.size(), size_t(1));
    QCOMPARE(f.engine.seekCalls.front(), long(30));
    QCOMPARE(f.controller.state(), PlaybackController::State::Playing);
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

void TestPlaybackController::seekWhilePausedIssuesEngineSeekAndKeepsState() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3", "/b.ogg"});
    f.controller.play();
    f.controller.pause();
    f.controller.seek(7);

    QCOMPARE(f.engine.seekCalls.size(), size_t(1));
    QCOMPARE(f.engine.seekCalls.front(), long(7));
    QCOMPARE(f.controller.state(), PlaybackController::State::Paused);
    // The engine position anchor followed the seek (display passthrough).
    QCOMPARE(f.controller.positionSeconds(), long(7));
}

void TestPlaybackController::seekWhileStoppedIsDocumentedNoop() {
    Fixture f;
    f.controller.loadQueue({"/a.mp3"});
    f.controller.play();
    f.controller.stop();
    f.controller.seek(15);

    // Documented no-op (SH-806): nothing is playing — the engine is not
    // touched and the rejection surfaces through the catalogue channel.
    QCOMPARE(f.engine.seekCalls.size(), size_t(0));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("seek: nothing playing"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Stopped);
}

void TestPlaybackController::seekWhileIdleIsDocumentedNoop() {
    Fixture f;
    f.controller.seek(4);

    QCOMPARE(f.engine.seekCalls.size(), size_t(0));
    QCOMPARE(f.invalidReasons.size(), size_t(1));
    QCOMPARE(lastReason(f.invalidReasons), QString("seek: nothing playing"));
    QCOMPARE(f.controller.state(), PlaybackController::State::Idle);
}

void TestPlaybackController::seekOnFreshRestoredSessionDropsTheRestoredOffset() {
    Fixture f;
    f.controller.restoreQueue({"/a.mp3", "/b.ogg"}, 0, 95);

    // A fresh restored session is Paused → the seek is a valid transport
    // command; the user's explicit position change ends the display-only
    // restored offset (the engine passthrough takes over).
    f.controller.seek(10);
    QCOMPARE(f.engine.seekCalls.size(), size_t(1));
    QCOMPARE(f.engine.seekCalls.front(), long(10));
    QCOMPARE(f.controller.positionSeconds(), long(10));
    QCOMPARE(f.controller.state(), PlaybackController::State::Paused);
    QCOMPARE(f.invalidReasons.size(), size_t(0));
}

QTEST_MAIN(TestPlaybackController)
#include "tst_playback_controller.moc"
