// SH-604 Phase B: PlayerFacade — the QObject wrapper bridging the pure-C++
// PlayerViewModel (and, for the application-layer wiring only, the
// PlaybackController) to QML. The QML layer sees ONLY this facade.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Contract conformance (PlayerViewModel.h):
//   - refresh() is the single re-read entry point. The QML shell calls
//     refresh() from a 1 Hz Timer; the controller's onStateChanged hook is
//     wired (by this facade, when constructed with a controller) to
//     refreshFromController(), the app layer's documented additional
//     refresh. All properties are snapshot getters with one shared NOTIFY
//     signal (updated), so every refresh publishes the whole snapshot.
//   - The view model never sees the controller's hooks; this facade owns
//     the wiring (PlaybackController is non-intrusively observed).
//   - onSeekRequested(clampedSeconds) is mirrored to the seekRequested()
//     signal; app/shanty.cpp connects it to the engine hop, which is a
//     documented NO-OP until the frozen IAudioEngine port gains a seek
//     primitive (SH-D604-B1, recorded at the wiring site).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "presentation/viewmodels/PlayerViewModel.h"

namespace shanty {

class PlaybackController;

class PlayerFacade : public QObject {
    Q_OBJECT

    /// "idle" | "playing" | "paused" | "stopped" (PlaybackController::State
    /// mapped for QML).
    Q_PROPERTY(QString state READ stateString NOTIFY updated)
    /// Empty string when nothing is selected.
    Q_PROPERTY(QString currentEntry READ currentEntry NOTIFY updated)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY updated)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY updated)
    /// SH-808 (UX-6): the queue's entries in queue order (plain paths), as
    /// of the last refresh() — the read-only source for the Player page's
    /// inline queue list; the highlight is currentIndex as an index into it.
    Q_PROPERTY(QVariantList queueEntries READ queueEntries NOTIFY updated)
    Q_PROPERTY(int positionSeconds READ positionSeconds NOTIFY updated)
    /// Track::kUnknownDuration (-1) when unknown.
    Q_PROPERTY(int durationSeconds READ durationSeconds NOTIFY updated)
    /// 0.0..1.0 with the view model's documented clamps.
    Q_PROPERTY(double progressFraction READ progressFraction NOTIFY updated)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY updated)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY updated)
    /// False exactly in the fatal-error stop state (transport gate).
    Q_PROPERTY(bool transportEnabled READ transportEnabled NOTIFY updated)
    Q_PROPERTY(bool hasError READ hasError NOTIFY updated)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY updated)
    Q_PROPERTY(QString lastErrorPath READ lastErrorPath NOTIFY updated)

public:
    /// @param controller optional: when given, the facade wires the
    ///     controller's onStateChanged (→ refresh) and onInvalidCommand
    ///     (→ invalidCommand signal) hooks — the application-layer wiring
    ///     the view model deliberately does not own.
    explicit PlayerFacade(PlayerViewModel& viewModel,
                          PlaybackController* controller = nullptr,
                          QObject* parent = nullptr);

    // ---- transport (thin forwards; invalid presses surface through
    //      invalidCommand(reason) as documented controller no-ops) ---------
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seekTo(int targetSeconds);
    /// Documented recovery after a fatal error (fresh-attempt play()).
    Q_INVOKABLE void retryPlayback();

    // ---- queue building (SH-602 selection → SH-604 playback bridge) -------
    /// Replaces the queue contents (selection → index 0); does NOT autoplay
    /// (PlaybackController::loadQueue contract).
    Q_INVOKABLE void loadQueue(const QVariantList& paths);
    /// (Re)starts playback of the current entry.
    Q_INVOKABLE void play();
    /// Selects AND plays the queue index (false cases surface through
    /// invalidCommand as documented controller no-ops).
    Q_INVOKABLE void jumpTo(int index);
    /// SH-802 (UX-2): the one queue-replace play command ("play a
    /// playlist"): empty list rejected through invalidCommand
    /// ("play: playlist is empty", state untouched); otherwise the queue is
    /// replaced in list order, index 0, playing — playStarted() fires only
    /// on success (navigation is the emitting page's decision).
    Q_INVOKABLE void playFromPaths(const QVariantList& paths);
    /// SH-805: the STARTUP session-restore announcement — emits
    /// confirmed("queue: restored") once the QML shell exists (the restore
    /// itself happens before any UI, so the application layer announces it
    /// through this facade method instead of a view-model command path).
    void notifySessionRestored();

    /// The single re-read entry point for the QML Timer; also called by the
    /// controller's onStateChanged wiring (see the class comment).
    Q_INVOKABLE void refresh();
    /// App-layer hook wired to controller.onStateChanged by the constructor.
    Q_INVOKABLE void refreshFromController();

    // ---- Q_PROPERTY accessors (snapshot getters) -----------------------------
    QString stateString() const;
    QString currentEntry() const;
    int currentIndex() const;
    int queueCount() const;
    QVariantList queueEntries() const;
    int positionSeconds() const;
    int durationSeconds() const;
    double progressFraction() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool transportEnabled() const;
    bool hasError() const;
    QString lastErrorMessage() const;
    QString lastErrorPath() const;

signals:
    /// Fired exactly once at the end of every refresh() (view-model
    /// onUpdated mirror); all snapshot properties share this NOTIFY.
    void updated();
    /// Rejected transport command (controller onInvalidCommand mirror).
    void invalidCommand(const QString& reason);
    /// Clamped seek command surfaced by the view model (see SH-D604-B1 at
    /// the wiring site in app/shanty.cpp for the engine hop).
    void seekRequested(int clampedSeconds);
    /// Fired instead of seekRequested when the duration is unknown.
    void seekRejected();
    /// SH-802: fired exactly once on queue-replace play success (the
    /// view-model onPlayStarted mirror; never on the rejection path).
    void playStarted();
    /// SH-803 (UX-7): the sparse positive-feedback mirror — only
    /// queue-building/restoring actions confirm (catalogue string in,
    /// shell maps it to the user-facing toast).
    void confirmed(const QString& message);

private:
    PlayerViewModel& viewModel_;
    PlaybackController* controller_;
};

}  // namespace shanty
