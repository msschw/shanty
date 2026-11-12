// Shanty — Sailfish OS local audio player. Application entry point: the
// object graph (SH-E6 Phase B, tickets SH-601..605) plus the QML
// registration of the controller facades.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Object graph (ISSUES.md §1.1 layering; dependencies point downwards):
//
//   adapters:  FilesystemScanner ──┐
//              FileIndexStore  ────┴→ LibraryService → LibraryViewModel
//              M3uPlaylistCodec → PlaylistService → PlaylistViewModel
//              AudioEngineAdapter → PlaybackQueue + PlaybackController →
//                                   PlayerViewModel
//              QSettingsSettingsStore ──┐
//              AutoEqReader          ───┴→ SettingsViewModel (owned by its
//                                            facade, see SettingsFacade.h)
//   facades:   NavigationFacade · LibraryFacade · PlaylistFacade ·
//              PlayerFacade · SettingsFacade
//
// Recorded wiring decisions:
//   SH-D605-B0 (settings store): the device ISettingsStore is the
//     QSettingsSettingsStore adapter (adapters/settings/, QtCore,
//     QSettings INI under the sandboxed GenericConfigLocation). The
//     architecture chart's "DcpSettingsStore" sketch described the port's
//     role, not a binding: Sailfish apps do not read dconf through QSettings
//     and the port contract only needs a key/value store, so the lean,
//     sandbox-safe QSettings file is the concrete choice (mirrors
//     FileIndexStore's SH-204 pattern).
//   SH-D402-6 (effect chain): AudioEngineAdapter implements IEffectChain as
//     a documented no-op/bypass (QtMultimedia on Sailfish exposes no
//     equalizer API — docs/engine-adapter.md). SettingsViewModel constructs
//     EffectChainService over this chain per apply (its decision 3), so no
//     standalone EffectChainService instance is pre-built in this graph.
//   SH-D604-B1 (seek): the frozen SH-402 IAudioEngine port defines no seek
//     primitive, so PlayerViewModel's clamped onSeekRequested command has no
//     engine hop yet — it is connected below as a documented NO-OP. The
//     view-model contract (clamp/reject) and the UI seek bar still work;
//     rewinding/forwarding on device lands once the port gains seek.
#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>

#include <sailfishapp.h>

#include "adapters/autoeq/AutoEqReader.h"
#include "adapters/engine/AudioEngineAdapter.h"
#include "adapters/filesystem/FilesystemScanner.h"
#include "adapters/m3u/M3uPlaylistCodec.h"
#include "adapters/settings/QSettingsSettingsStore.h"
#include "adapters/store/FileIndexStore.h"
#include "adapters/store/QueueStateStore.h"  // SH-805 (UX-4) session store
#include "core/playback/PlaybackController.h"
#include "core/playback/PlaybackQueue.h"
#include "core/services/LibraryService.h"
#include "core/services/PlaylistService.h"
#include "controllers/LibraryFacade.h"
#include "controllers/NavigationFacade.h"
#include "controllers/PlayerFacade.h"
#include "controllers/PlaylistFacade.h"
#include "controllers/SettingsFacade.h"
#include "presentation/viewmodels/LibraryViewModel.h"
#include "presentation/viewmodels/NavigationViewModel.h"
#include "presentation/viewmodels/PlayerViewModel.h"
#include "presentation/viewmodels/PlaylistViewModel.h"

int main(int argc, char *argv[])
{
    using namespace shanty;

    QScopedPointer<QGuiApplication> application(
        SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());

    // ---- adapters + services -------------------------------------------------
    FilesystemScanner scanner;
    FileIndexStore libraryStore;             // SH-204 file-backed library config
    LibraryService libraryService(scanner);  // SH-202 scan + filter composition
    M3uPlaylistCodec playlistCodec;          // SH-301..303 .m3u/.m3u8 codec
    PlaylistService playlistService(playlistCodec);
    AudioEngineAdapter engine;               // IAudioEngine + IEffectChain no-op
    PlaybackQueue queue;                     // SH-401 stop-at-end queue logic
    PlaybackController controller(queue, engine);  // SH-403/404 transport
    QSettingsSettingsStore settingsStore;    // SH-D605-B0 (see header comment)
    AutoEqReader autoEqReader;               // SH-503 EqualizerAPO/CSV reader
    QueueStateStore queueStore;              // SH-805 (UX-4) session store

    // SH-805 (UX-4): adopt the persisted session (if any) BEFORE the landing
    // decision below — a restored queue makes the startup landing the Player
    // page (SH-801's promise), paused at the saved track (restore-paused
    // invariant: restoring never starts audio on its own). A missing/corrupt
    // store yields an empty state → no restore → Library landing (first run).
    const QueueState restoredSession = queueStore.load();
    if (!restoredSession.paths.empty()) {
        controller.restoreQueue(restoredSession.paths, restoredSession.index,
                                restoredSession.positionSeconds);
    }

    // ---- view models (pure C++; callbacks are mirrored by the facades) ------
    NavigationViewModel navigationViewModel;
    LibraryViewModel libraryViewModel(libraryStore, libraryService);
    PlaylistViewModel playlistViewModel(playlistService);
    PlayerViewModel playerViewModel(controller, queue);
    // SettingsViewModel is owned by SettingsFacade (SH-D605-B1: the facade
    // reconstructs it over the same store as its documented reload path).

    // SH-805 (found during the device drive): the view-model's getters are
    // SNAPSHOT getters (PlayerViewModel.h contract — they follow the last
    // refresh()), and nothing has refreshed yet at startup. Drive the
    // documented single re-read entry point ONCE here — after the session
    // restore above, before the facades — so the landing decision below
    // (and the cover's startup data) see the restored session instead of
    // the empty initial snapshot.
    playerViewModel.refresh();

    // ---- facades (QML controllers; parented to the application) -------------
    // The facades mirror the view-model callbacks, so they must exist before
    // any QML code runs; the graph below is therefore fully wired here.
    // SH-801 (UX-1): the landing decision is made once from the player's
    // startup queue state (a session queue — or, once SH-805 lands, a
    // restored one). The shell applies the decision as an explicit startup
    // push of the Player page over the fresh Library root.
    NavigationFacade *navigation = new NavigationFacade(
        navigationViewModel, playerViewModel.queueCount() > 0,
        application.data());
    LibraryFacade *library = new LibraryFacade(
        libraryViewModel, application.data());
    PlaylistFacade *playlist = new PlaylistFacade(
        playlistViewModel, application.data());
    PlayerFacade *player = new PlayerFacade(
        playerViewModel, &controller, application.data());
    SettingsFacade *settings = new SettingsFacade(
        settingsStore, engine, autoEqReader, application.data());

    // SH-D604-B1: the clamped seek command surfaces here; the engine has no
    // seek primitive yet (frozen IAudioEngine port), so the hop is a
    // documented no-op. Everything else of the seek contract (clamping,
    // rejection, the seek bar's snapshot duration) already works.
    QObject::connect(player, &PlayerFacade::seekRequested,
                     player, [](int /*clampedSeconds*/) {
                         // SH-D604-B1: no engine seek primitive yet.
                     });

    // SH-802 (UX-2): the "play a playlist" bridge — PlaylistFacade extracts
    // the entry paths and this connection routes them to the player side's
    // queue-replace play command (the empty-check decision lives in
    // PlayerViewModel::playFromPaths; the rejections surface through the
    // player facade's invalidCommand → shell toast).
    QObject::connect(playlist, &PlaylistFacade::playRequested, player,
                     [player](const QVariantList& paths) {
                         player->playFromPaths(paths);
                     });

    // SH-805 (UX-4): the persistence trigger. The shell's 1 Hz refresh (plus
    // every controller-driven refresh) fires PlayerFacade::updated; each
    // tick saves a full snapshot — queue mutation, track change and position
    // ticks are all covered by that cadence, and the store's trailing-write
    // debounce coalesces the writes (the store always ends up with the
    // latest snapshot; the destructor flushes a pending one on shutdown).
    QObject::connect(player, &PlayerFacade::updated, player,
                     [&queueStore, &controller, &queue, player]() {
                         QueueState state;
                         state.paths = queue.entries();
                         state.index = queue.currentIndex();
                         state.positionSeconds = controller.positionSeconds();
                         state.transportState =
                             player->stateString().toStdString();
                         queueStore.save(state);
                     });

    // ---- QML registration -----------------------------------------------------
    // Decision (SH-E6 Phase B): context properties on the view's ROOT
    // context, one per facade — instead of qmlRegisterSingletonType. Rationale:
    // the facades are application-lifetime singletons owned by main() anyway;
    // root-context properties are visible in every page AND in the cover
    // page with zero per-type registration boilerplate, and they keep the
    // facades out of the QML import namespace (they are controllers, not
    // QML types).
    QQmlContext *rootContext = view->rootContext();
    // Wave-8c: registered as "navigationFacade" — NOT "navigation". Silica's
    // PageStack injects a per-page context property literally named
    // "navigation" (the push's int PageNavigation style, 0 = NoNavigation)
    // into every page pushed through pageStack.push(url), shadowing a
    // root-context property of the same name; the LibraryPage (created from
    // a Component) kept the real facade, which is why row taps on
    // PlaylistPage read `navigation` as 0 on the device. App-owned name,
    // no collision.
    rootContext->setContextProperty(QStringLiteral("navigationFacade"), navigation);
    rootContext->setContextProperty(QStringLiteral("library"), library);
    rootContext->setContextProperty(QStringLiteral("playlist"), playlist);
    rootContext->setContextProperty(QStringLiteral("player"), player);
    rootContext->setContextProperty(QStringLiteral("settings"), settings);

    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    // SH-803/SH-805: the restored-session confirmation is a STARTUP
    // announcement — the restore itself happened before any UI existed (the
    // landing decision needs it), so the application layer announces it
    // through the facade's confirmation channel only now that the shell's
    // Connections exist (mapped to the toast by shanty.qml's reasonMessage).
    if (!restoredSession.paths.empty()) {
        player->notifySessionRestored();
    }

    return application->exec();
}
