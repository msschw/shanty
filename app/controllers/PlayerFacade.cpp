// SH-604 Phase B: PlayerFacade implementation (see the header).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlayerFacade.h"

#include "core/playback/PlaybackController.h"

namespace shanty {

PlayerFacade::PlayerFacade(PlayerViewModel& viewModel,
                           PlaybackController* controller, QObject* parent)
    : QObject(parent), viewModel_(viewModel), controller_(controller) {
    // Mirror the view model's hooks.
    viewModel_.onSeekRequested = [this](long clampedSeconds) {
        emit seekRequested(static_cast<int>(clampedSeconds));
    };
    viewModel_.onSeekRejected = [this] { emit seekRejected(); };
    viewModel_.onUpdated = [this] { emit updated(); };
    // SH-802: the queue-replace play command's own rejection/success hooks
    // share the controller rejections' QML channels (invalidCommand → shell
    // toast; playStarted → the emitting page's navigation).
    viewModel_.onInvalidCommand = [this](const char* reason) {
        emit invalidCommand(QString::fromUtf8(reason));
    };
    viewModel_.onPlayStarted = [this] { emit playStarted(); };
    // SH-803: the confirmation channel shares the rejection channel's QML
    // surface (shell toast); the catalogue string stays VM-owned.
    viewModel_.onConfirmed = [this](const char* message) {
        emit confirmed(QString::fromUtf8(message));
    };

    if (controller_ != nullptr) {
        // Application-layer wiring (the view model is non-intrusive and
        // never assigns these hooks): every controller state transition
        // triggers a view-model refresh, invalid commands surface as a
        // signal for the shell's toasts.
        controller_->onStateChanged = [this] { refreshFromController(); };
        controller_->onInvalidCommand = [this](const char* reason) {
            emit invalidCommand(QString::fromUtf8(reason));
        };
    }
}

void PlayerFacade::togglePlayPause() { viewModel_.togglePlayPause(); }

void PlayerFacade::next() { viewModel_.next(); }

void PlayerFacade::previous() { viewModel_.previous(); }

void PlayerFacade::seekTo(int targetSeconds) {
    viewModel_.seekTo(static_cast<long>(targetSeconds));
}

void PlayerFacade::retryPlayback() { viewModel_.retryPlayback(); }

void PlayerFacade::loadQueue(const QVariantList& paths) {
    std::vector<std::string> entries;
    entries.reserve(static_cast<std::size_t>(paths.size()));
    for (const QVariant& path : paths) {
        entries.push_back(path.toString().toStdString());
    }
    if (controller_ != nullptr) {
        controller_->loadQueue(entries);
    }
}

void PlayerFacade::play() {
    if (controller_ != nullptr) {
        controller_->play();
    }
}

void PlayerFacade::jumpTo(int index) {
    if (controller_ != nullptr && index >= 0) {
        controller_->jumpTo(static_cast<std::size_t>(index));
    }
}

void PlayerFacade::playFromPaths(const QVariantList& paths) {
    std::vector<std::string> entries;
    entries.reserve(static_cast<std::size_t>(paths.size()));
    for (const QVariant& path : paths) {
        entries.push_back(path.toString().toStdString());
    }
    viewModel_.playFromPaths(entries);
}

void PlayerFacade::notifySessionRestored() {
    // SH-805: the SH-803-reserved confirmation wording, verbatim.
    emit confirmed(QStringLiteral("queue: restored"));
}

void PlayerFacade::refresh() { viewModel_.refresh(); }

void PlayerFacade::refreshFromController() { viewModel_.refresh(); }

QString PlayerFacade::stateString() const {
    switch (viewModel_.state()) {
        case PlaybackController::State::Playing:
            return QStringLiteral("playing");
        case PlaybackController::State::Paused:
            return QStringLiteral("paused");
        case PlaybackController::State::Stopped:
            return QStringLiteral("stopped");
        case PlaybackController::State::Idle:
            break;
    }
    return QStringLiteral("idle");
}

QString PlayerFacade::currentEntry() const {
    return QString::fromStdString(viewModel_.currentEntry());
}

int PlayerFacade::currentIndex() const {
    return static_cast<int>(viewModel_.currentIndex());
}

int PlayerFacade::queueCount() const {
    return static_cast<int>(viewModel_.queueCount());
}

int PlayerFacade::positionSeconds() const {
    return static_cast<int>(viewModel_.positionSeconds());
}

int PlayerFacade::durationSeconds() const {
    return static_cast<int>(viewModel_.durationSeconds());
}

double PlayerFacade::progressFraction() const {
    return viewModel_.progressFraction();
}

bool PlayerFacade::canGoNext() const { return viewModel_.canGoNext(); }

bool PlayerFacade::canGoPrevious() const {
    return viewModel_.canGoPrevious();
}

bool PlayerFacade::transportEnabled() const {
    return viewModel_.transportEnabled();
}

bool PlayerFacade::hasError() const { return viewModel_.hasError(); }

QString PlayerFacade::lastErrorMessage() const {
    return QString::fromStdString(viewModel_.lastErrorMessage());
}

QString PlayerFacade::lastErrorPath() const {
    return QString::fromStdString(viewModel_.lastErrorPath());
}

}  // namespace shanty
