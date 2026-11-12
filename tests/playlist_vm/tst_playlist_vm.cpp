// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
// SH-603 Phase A/B: playlist view-model suite (Epic SH-E6). Exercises the
// PlaylistService facade (import/export + resolution at the boundary +
// mutation passthrough) and the PlaylistViewModel import/export dialog
// flow state machine, against the scriptable FakePlaylistCodec. The fake
// is trusted (see tests/m3u_* for the real codec); the units under test
// are the facade and the view model.
// Phase A ran against the SH-603 stubs: every case failed by ASSERTION
// (QFATAL count 0, never by crash) — indexed observations are crash-proof
// helpers so a failing QCOMPARE cannot turn into a vector::at throw.
#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include <cstddef>
#include <string>
#include <vector>

#include "services/PlaylistService.h"
#include "presentation/viewmodels/PlaylistViewModel.h"
#include "FakePlaylistCodec.h"

using shanty::FakePlaylistCodec;
using shanty::PlaylistEntry;
using shanty::PlaylistFlowState;

namespace {

QString asQ(const std::string& s) { return QString::fromStdString(s); }

const char* flowName(PlaylistFlowState state) {
    switch (state) {
        case PlaylistFlowState::Idle: return "Idle";
        case PlaylistFlowState::AwaitingConfirm: return "AwaitingConfirm";
        case PlaylistFlowState::AwaitingFile: return "AwaitingFile";
        case PlaylistFlowState::Importing: return "Importing";
        case PlaylistFlowState::Exporting: return "Exporting";
        case PlaylistFlowState::Done: return "Done";
        case PlaylistFlowState::Failed: return "Failed";
    }
    return "?";
}

bool witnessFileBytes(const QString& path, const std::string& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    return file.readAll() == QByteArray::fromStdString(bytes);
}

PlaylistEntry pe(const std::string& path, const std::string& title,
                 int durationSeconds) {
    PlaylistEntry entry;
    entry.path = path;
    entry.title = title;
    entry.durationSeconds = durationSeconds;
    return entry;
}

/// Crash-proof observation helpers: a failing size QCOMPARE continues the
/// test function, so indexed log reads must tolerate empty logs (Phase A).
QString lastReason(const std::vector<std::string>& reasons) {
    return reasons.empty() ? QString() : asQ(reasons.back());
}
QString firstReason(const std::vector<std::string>& reasons) {
    return reasons.empty() ? QString() : asQ(reasons.front());
}
QString lastEvent(const std::vector<std::string>& events) {
    return events.empty() ? QString() : asQ(events.back());
}
QString firstEvent(const std::vector<std::string>& events) {
    return events.empty() ? QString() : asQ(events.front());
}
/// firstEvent as a raw expected C-string comparison (std::string form).
QString firstEventR(const std::vector<std::string>& events) {
    return asQ(events.empty() ? std::string() : events.front());
}

/// Fresh codec + service + view model + event log per test. The combined
/// event log interleaves flow transitions ("flow:<State>") and playlist
/// change notifications ("changed:<count at callback time>") so the
/// documented callback ordering guarantees ("mutated → then changed",
/// import-finished before playlist-changed) become assertable.
struct Fixture {
    QTemporaryDir dir;  // readable parent directory + temp export target
    FakePlaylistCodec codec;
    shanty::PlaylistService service{codec};
    shanty::PlaylistViewModel vm{service};

    std::vector<std::string> order;         ///< interleaved event log
    std::vector<std::string> invalidReasons;  ///< onInvalidCommand log

    QString importQPath() const { return dir.filePath("list.m3u"); }
    std::string importPath() const { return importQPath().toStdString(); }
    QString exportQPath() const { return dir.filePath("out.m3u"); }
    std::string exportPath() const { return exportQPath().toStdString(); }

    Fixture() {
        vm.onFlowChanged = [this] {
            order.push_back(std::string("flow:") + flowName(vm.flowState()));
        };
        vm.onEntriesChanged = [this] {
            order.push_back("changed:" + std::to_string(vm.entryCount()));
        };
        vm.onInvalidCommand = [this](const char* reason) {
            invalidReasons.push_back(reason ? reason : "(null)");
        };
    }

    /// Creates a readable playlist file at the import path (the service's
    /// documented contract: codec.parse is only reached when the file can
    /// be opened — unreadable files fail before the codec).
    void seedImportFile() const {
        QFile file(importQPath());
        file.open(QIODevice::WriteOnly);
        file.write("#EXTM3U\nseed bytes for the fake codec\n");
    }
};

}  // namespace

class TestPlaylistVM : public QObject {
    Q_OBJECT

private slots:
    // ---- flow state machine: entry points & rejections ---------------------
    void freshViewModelIsIdleWithNoEvents();
    void beginImportStartsAwaitingFlowFromFileStates();
    void confirmWithoutBeginIsInvalidNoop();
    void beginWhileFlowOpenIsInvalidNoop();
    void pendingOpMismatchIsRejected();
    void doneStateRequiresNewBeginBeforeNextConfirm();

    // ---- import flow (fake codec) ------------------------------------------
    void successfulImportRunsAwaitingFileImportingDone();
    void importResolvesFileUrlAndAbsoluteEntriesAtImportBoundary();
    void importBaseDirWithoutSlashLeavesRelativeEntries();
    void failedImportByCodecRecordsErrorAndKeepsPlaylist();
    void unreadableFileImportFailsWithoutCallingCodec();

    // ---- cancel flow ---------------------------------------------------------
    void cancelRulesAsDocumented();
    void cancelFromImportingHopIsRejectedReentrantly();

    // ---- export flow (fake codec + real file writes) ------------------------
    void successfulExportWritesBytesAndCarriesPlaylistToCodec();
    void exportOverwritesExistingTargetUnconditionally();
    void exportSerializeErrorIsRecordedWithoutFile();
    void exportUnwritableTargetIsRecorded();

    // ---- mutation delegation (SH-304 facade) --------------------------------
    void addDelegatesAndNotifiesExactlyOnce();
    void moveRemoveClearDelegateAndNotifyUpdatedState();
    void domainNoopMutationsStillNotifyChangedWithStableState();
    void deduplicateKeepsFirstOccurrenceThroughFacade();
    void totalDurationPassesThroughFacade();

    // ---- full chain ----------------------------------------------------------
    void importEditExportChainPreservesMetadataRoundTrip();

    // ---- SH-807 (UX-11): playlist dirty guard (red-first) --------------------
    void freshViewModelIsNotDirty();
    void dirtySetByMutationsThatChangeEntries();
    void domainNoopMutationsDoNotSetDirty();
    void deduplicateSetsDirtyOnlyWhenEntriesChange();
    void beginImportWhileDirtyAwaitsConfirmAndConfirmDiscardProceeds();
    void confirmDiscardOutsideAwaitingConfirmIsRejectedNoop();
    void cancelFromAwaitingConfirmKeepsEditsAndDirty();
    void beginExportWhileDirtyIsNotGuarded();
    void successfulImportClearsDirtyFailedImportKeepsIt();
    void successfulExportClearsDirtyFailedExportKeepsIt();
};

void TestPlaylistVM::freshViewModelIsIdleWithNoEvents() {
    Fixture f;
    // Documented: no flow was opened, no event fired, no failure recorded.
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    QVERIFY(f.order.empty());
    QVERIFY(f.invalidReasons.empty());
    QVERIFY(f.vm.lastError().empty());
    QCOMPARE(f.vm.entryCount(), size_t(0));
    // Idle view model never touches the codec.
    QVERIFY(f.codec.parseFilePaths.empty());
    QVERIFY(f.codec.serializeTargets.empty());
}

void TestPlaylistVM::beginImportStartsAwaitingFlowFromFileStates() {
    Fixture f;
    // beginImport from Idle opens the dialog flow.
    f.vm.beginImport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    // cancelFlow dismisses it → Idle.
    f.vm.cancelFlow();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    // beginExport likewise opens the flow (export target pending).
    f.vm.beginExport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    f.vm.cancelFlow();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    // Every step was an actual transition, nothing was rejected.
    QCOMPARE(f.order.size(), size_t(4));
    if (f.order.size() == 4) {
        QCOMPARE(firstEvent(f.order), QString("flow:AwaitingFile"));
        // The LIST of transitions ends Idle; the AwaitingFile hop at index
        // 2 is the second begin's transition.
        QCOMPARE(asQ(f.order[2]), QString("flow:AwaitingFile"));
        QCOMPARE(lastEvent(f.order), QString("flow:Idle"));
    }
    QVERIFY(f.invalidReasons.empty());
    QCOMPARE(f.vm.entryCount(), size_t(0));
}

void TestPlaylistVM::confirmWithoutBeginIsInvalidNoop() {
    Fixture f;
    f.vm.confirmImport(f.importPath());
    f.vm.confirmExport(f.exportPath());
    // Documented: confirm only at AwaitingFile is valid; from Idle both are
    // rejected no-ops (state Idle preserved, no flow transitions recorded).
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    QVERIFY(f.order.empty());
    QCOMPARE(f.invalidReasons.size(), size_t(2));
    QCOMPARE(firstReason(f.invalidReasons),
             QString("confirmImport: not awaiting file"));
    QCOMPARE(lastReason(f.invalidReasons),
             QString("confirmExport: not awaiting file"));
    // The codec was never called.
    QVERIFY(f.codec.parseFilePaths.empty());
    QVERIFY(f.codec.serializeTargets.empty());
}

void TestPlaylistVM::beginWhileFlowOpenIsInvalidNoop() {
    Fixture f;
    f.vm.beginImport();
    const size_t expectedEvents = f.order.size();
    // Already AwaitingFile: a second begin is rejected, state preserved.
    f.vm.beginImport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    QCOMPARE(f.order.size(), expectedEvents);
    QCOMPARE(lastReason(f.invalidReasons),
             QString("beginImport: flow already open"));

    f.vm.cancelFlow();
    f.vm.beginImport();
    // Pending import flow: beginExport must not reopen/steal the dialog.
    f.vm.beginExport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    QCOMPARE(lastReason(f.invalidReasons),
             QString("beginExport: flow already open"));
}

void TestPlaylistVM::pendingOpMismatchIsRejected() {
    Fixture f;
    // Begin EXPORT, then try to confirm an import.
    f.vm.beginExport();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    QCOMPARE(lastReason(f.invalidReasons),
             QString("confirmImport: export pending"));
    QVERIFY(f.codec.parseFilePaths.empty());

    // The pending export still completes normally afterwards.
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);

    // Symmetric: begin IMPORT, then try to confirm an export.
    f.vm.beginImport();
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    QCOMPARE(lastReason(f.invalidReasons),
             QString("confirmExport: import pending"));
    // The only serialize call so far was the successful export above.
    QCOMPARE(f.codec.serializeTargets.size(), size_t(1));
}

void TestPlaylistVM::doneStateRequiresNewBeginBeforeNextConfirm() {
    Fixture f;
    shanty::ParseResult parsed;
    parsed.playlist.add(pe("/a.mp3", "A", 10));
    f.codec.nextParse = parsed;
    f.seedImportFile();
    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);

    // Documented: done state is a resting state; confirm again → invalid.
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(lastReason(f.invalidReasons),
             QString("confirmImport: not awaiting file"));
    // A new beginImport re-opens the flow from Done.
    f.vm.beginImport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
}

void TestPlaylistVM::successfulImportRunsAwaitingFileImportingDone() {
    Fixture f;
    shanty::ParseResult parsed;
    // Relative entry + metadata: resolution (SH-305/D3) belongs to the
    // service at the import boundary; metadata must be carried through.
    parsed.playlist.add(pe("song.mp3", "Song", 180));
    f.codec.nextParse = parsed;
    f.seedImportFile();

    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());

    // Synchronous service: Done before confirmImport returns.
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QVERIFY(f.vm.lastError().empty());
    // State machine: Idle→AwaitingFile→Importing→Done, then ONE change event
    // (import-finished before playlist-changed; count already applied).
    QCOMPARE(f.order.size(), size_t(4));
    if (f.order.size() == 4) {
        QCOMPARE(asQ(f.order[0]), QString("flow:AwaitingFile"));
        QCOMPARE(asQ(f.order[1]), QString("flow:Importing"));
        QCOMPARE(asQ(f.order[2]), QString("flow:Done"));
        QCOMPARE(asQ(f.order[3]), QString("changed:1"));
    }
    // The codec received the exact requested file path.
    QCOMPARE(f.codec.parseFilePaths.size(), size_t(1));
    if (f.codec.parseFilePaths.size() == 1) {
        QCOMPARE(asQ(f.codec.parseFilePaths.front()), f.importQPath());
    }
    // D3: domain holds the resolved plain path (relative → parent dir).
    QCOMPARE(f.vm.entryCount(), size_t(1));
    if (f.vm.entryCount() == 1) {
        const PlaylistEntry& e = f.vm.entries().at(0);
        QCOMPARE(asQ(e.path), f.importQPath().section('/', 0, -2) +
                                  QString::fromLatin1("/song.mp3"));
        QCOMPARE(asQ(e.title), QString::fromLatin1("Song"));
        QCOMPARE(e.durationSeconds, 180);
    }
    QCOMPARE(f.vm.totalDurationSeconds(), 180LL);
}

void TestPlaylistVM::importResolvesFileUrlAndAbsoluteEntriesAtImportBoundary() {
    Fixture f;
    shanty::ParseResult parsed;
    // file:// URL with percent-encoded UTF-8 (SH-305), absolute path
    // (passes through), and another relative entry resolved against the
    // playlist's parent directory.
    parsed.playlist.add(pe("file:///music/%C3%B6.flac", "Song C", 240));
    parsed.playlist.add(pe("/abs/x.ogg", "", 97));
    parsed.playlist.add(pe("sub/song_b.ogg", "", -1));
    f.codec.nextParse = parsed;
    f.seedImportFile();

    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.vm.entryCount(), size_t(3));
    if (f.vm.entryCount() == 3) {
        const auto& es = f.vm.entries();
        QCOMPARE(asQ(es.at(0).path),
                 QString::fromUtf8("/music/\xc3\xb6.flac"));
        QCOMPARE(asQ(es.at(0).title), QString::fromLatin1("Song C"));
        QCOMPARE(asQ(es.at(1).path), QString::fromLatin1("/abs/x.ogg"));
        QCOMPARE(asQ(es.at(2).path), f.importQPath().section('/', 0, -2) +
                                         QString::fromLatin1("/sub/song_b.ogg"));
        QCOMPARE(es.at(2).durationSeconds, -1);
    }
    // Total duration flags unknown entries as unknown (SH-304 contract).
    QCOMPARE(f.vm.totalDurationSeconds(), (long long)shanty::Track::kUnknownDuration);
}

void TestPlaylistVM::importBaseDirWithoutSlashLeavesRelativeEntries() {
    Fixture f;
    shanty::ParseResult parsed;
    parsed.playlist.add(pe("song.mp3", "S", 5));
    f.codec.nextParse = parsed;
    f.seedImportFile();

    // No '/' in the given path → no parent directory → relative entries
    // stay untouched (PathResolver's documented baseDir-"" rule). The file
    // itself must exist relative to the process CWD.
    QFile seeded(QStringLiteral("list.m3u"));
    QCOMPARE(seeded.open(QIODevice::WriteOnly), true);
    seeded.write("#EXTM3U\n");
    f.vm.beginImport();
    f.vm.confirmImport("list.m3u");
    seeded.remove();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.vm.entryCount(), size_t(1));
    if (f.vm.entryCount() == 1) {
        QCOMPARE(asQ(f.vm.entries().at(0).path), QString("song.mp3"));
    }
}

void TestPlaylistVM::failedImportByCodecRecordsErrorAndKeepsPlaylist() {
    Fixture f;
    // Seed the playlist with one entry first.
    f.vm.add(pe("/a.mp3", "A", 10));
    QCOMPARE(f.vm.entryCount(), size_t(1));

    shanty::ParseResult bad;
    bad.errors.push_back(shanty::ParseError{shanty::ParseError::Code::NotAPlaylist,
                                            "not a playlist garbage", 3});
    f.codec.nextParse = bad;
    f.seedImportFile();  // readable file: the codec IS called and errs

    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());

    // Documented: codec errors → Failed, service message = codec's first
    // error message; the playlist stays UNCHANGED (no change event fired).
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(asQ(f.vm.lastError()), QString("not a playlist garbage"));
    QCOMPARE(f.order.size(), size_t(4));
    if (f.order.size() == 4) {
        QCOMPARE(asQ(f.order[0]), QString("changed:1"));  // from the seed add
        QCOMPARE(asQ(f.order[1]), QString("flow:AwaitingFile"));
        QCOMPARE(asQ(f.order[2]), QString("flow:Importing"));
        QCOMPARE(asQ(f.order[3]), QString("flow:Failed"));
    }
    QCOMPARE(f.vm.entryCount(), size_t(1));
    if (f.vm.entryCount() == 1) {
        QCOMPARE(f.vm.entries().at(0), pe("/a.mp3", "A", 10));
    }
}

void TestPlaylistVM::unreadableFileImportFailsWithoutCallingCodec() {
    Fixture f;
    const QString missing = f.dir.filePath("missing.m3u");
    f.vm.beginImport();
    f.vm.confirmImport(missing.toStdString());

    // Documented: unreadable files are detected by the service alone —
    // message "<path>: cannot open for reading", the codec is NOT called,
    // the flow ends Failed.
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(asQ(f.vm.lastError()),
             missing + QString::fromLatin1(": cannot open for reading"));
    QVERIFY(f.codec.parseFilePaths.empty());
    QCOMPARE(f.vm.entryCount(), size_t(0));
    QVERIFY(f.order.empty() == false);  // the flow transitions fired
    QCOMPARE(f.vm.entryCount(), size_t(0));
    // No change event: nothing was applied.
    for (const std::string& event : f.order) {
        QVERIFY2(asQ(event).startsWith(QString("flow:")),
                 qPrintable(QString("unexpected change event: %1")
                                .arg(QString::fromStdString(event))));
    }
}

void TestPlaylistVM::cancelRulesAsDocumented() {
    Fixture f;
    // Idle → cancel is a documented invalid no-op.
    f.vm.cancelFlow();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    QCOMPARE(lastReason(f.invalidReasons), QString("cancel: no open flow"));

    // AwaitingFile → Idle.
    f.vm.beginImport();
    f.vm.cancelFlow();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);

    // Done → Idle (result dismissed).
    shanty::ParseResult parsed;
    parsed.playlist.add(pe("/a.mp3", "A", 10));
    f.codec.nextParse = parsed;
    f.seedImportFile();
    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    f.vm.cancelFlow();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);

    // Failed → Idle (error dismissed).
    shanty::ParseResult bad;
    bad.errors.push_back(shanty::ParseError{shanty::ParseError::Code::NotAPlaylist,
                                            "bad", 1});
    f.codec.nextParse = bad;
    f.vm.beginImport();
    QCOMPARE(asQ(f.vm.lastError()), QString());  // fresh attempt: cleared
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(asQ(f.vm.lastError()), QString("bad"));
    f.vm.cancelFlow();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    // The error message survives in observation only while Failed.
    QCOMPARE(asQ(f.vm.lastError()), QString());
}

void TestPlaylistVM::cancelFromImportingHopIsRejectedReentrantly() {
    Fixture f;
    shanty::ParseResult parsed;
    parsed.playlist.add(pe("/a.mp3", "A", 10));
    f.codec.nextParse = parsed;
    f.seedImportFile();
    // Reentrancy probe: cancel DURING the Importing transition (the only
    // way UI code could observe the synchronous hop) is the documented
    // invalid no-op "cancel: flow in progress"; the import still completes.
    f.vm.onFlowChanged = [&f] {
        f.order.push_back(std::string("flow:") + flowName(f.vm.flowState()));
        if (f.vm.flowState() == PlaylistFlowState::Importing) {
            f.vm.cancelFlow();
        }
    };
    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(asQ(f.invalidReasons.back()),
             QString("cancel: flow in progress"));
    QCOMPARE(f.vm.entryCount(), size_t(1));
    // AwaitingFile → Importing → Done, then the applied playlist change —
    // the rejected cancel fired nothing.
    QCOMPARE(f.order.size(), size_t(4));
    if (f.order.size() == 4) {
        QCOMPARE(lastEvent(f.order), QString("changed:1"));
        QCOMPARE(asQ(f.order[2]), QString("flow:Done"));
    }
}

void TestPlaylistVM::successfulExportWritesBytesAndCarriesPlaylistToCodec() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "Song A", 180));
    f.vm.add(pe("/b.ogg", "Song B", 97));
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Success;
    f.codec.nextSerialize.data = "#EXTM3U\n#EXTINF:180,Song A\n/a.mp3\n";

    f.vm.beginExport();
    f.vm.confirmExport(f.exportPath());

    // Done, no error, and the playlist was untouched (no change events).
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QVERIFY(f.vm.lastError().empty());
    QCOMPARE(firstEventR(f.order), "changed:1");
    QCOMPARE(lastEvent(f.order), QString("flow:Done"));

    // The codec received the target path and the exact playlist (metadata
    // round trip per the SH-303 foundation).
    QCOMPARE(f.codec.serializeTargets.size(), size_t(1));
    if (f.codec.serializeTargets.size() == 1) {
        QCOMPARE(asQ(f.codec.serializeTargets.front()), f.exportQPath());
        QCOMPARE(f.codec.serializePlaylists.front().count(), size_t(2));
        if (f.codec.serializePlaylists.front().count() == 2) {
            const auto& es = f.codec.serializePlaylists.front().entries();
            QCOMPARE(es.at(0), pe("/a.mp3", "Song A", 180));
            QCOMPARE(es.at(1), pe("/b.ogg", "Song B", 97));
        }
    }
    // The serialized bytes were written verbatim to the target file.
    QFile written(f.exportQPath());
    QCOMPARE(written.open(QIODevice::ReadOnly), true);
    QCOMPARE(written.readAll(),
             QByteArray::fromStdString(f.codec.nextSerialize.data));
}

void TestPlaylistVM::exportOverwritesExistingTargetUnconditionally() {
    Fixture f;
    const QString target = f.exportQPath();
    QFile stale(target);
    QCOMPARE(stale.open(QIODevice::WriteOnly), true);
    stale.write("stale old content that must not survive\n");
    stale.close();

    f.vm.add(pe("/x.ogg", "", 30));
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Success;
    f.codec.nextSerialize.data = "#EXTM3U\n#EXTINF:30,\n/x.ogg\n";

    f.vm.beginExport();
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);

    // Documented: an existing target is truncated and overwritten
    // unconditionally (overwrite confirmation belongs to the dialog flow).
    QFile written(target);
    QCOMPARE(written.open(QIODevice::ReadOnly), true);
    QCOMPARE(written.readAll(),
             QByteArray::fromStdString(f.codec.nextSerialize.data));
}

void TestPlaylistVM::exportSerializeErrorIsRecordedWithoutFile() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Error;
    f.codec.nextSerialize.errorMessage = "cannot encode this playlist";

    f.vm.beginExport();
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(asQ(f.vm.lastError()), QString("cannot encode this playlist"));
    // The codec was asked exactly once, nothing was written, the playlist
    // was not touched (no change events).
    QCOMPARE(f.codec.serializeTargets.size(), size_t(1));
    QVERIFY(!QFile::exists(f.exportQPath()));
    QCOMPARE(firstEventR(f.order), "changed:1");  // the seed add only
    QCOMPARE(lastEvent(f.order), QString("flow:Failed"));
}

void TestPlaylistVM::exportUnwritableTargetIsRecorded() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Success;
    f.codec.nextSerialize.data = "#EXTM3U\n";
    const QString target = f.dir.filePath("no_such_dir/out.m3u");

    f.vm.beginExport();
    f.vm.confirmExport(target.toStdString());
    // Documented: a target that cannot be opened → UnwritableFile surfaced
    // as "<path>: cannot open for writing" (the codec already succeeded).
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(asQ(f.vm.lastError()),
             target + QString::fromLatin1(": cannot open for writing"));
}

void TestPlaylistVM::addDelegatesAndNotifiesExactlyOnce() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));
    QCOMPARE(f.vm.entryCount(), size_t(1));
    f.vm.add(pe("/b.ogg", "B", 20));
    QCOMPARE(f.vm.entryCount(), size_t(2));
    // Mutated → then changed, exactly once per delegate call, with the
    // count already observable at callback time.
    QCOMPARE(f.order.size(), size_t(2));
    if (f.order.size() == 2) {
        QCOMPARE(asQ(f.order[0]), QString("changed:1"));
        QCOMPARE(asQ(f.order[1]), QString("changed:2"));
    }
    if (f.vm.entryCount() == 2) {
        QCOMPARE(f.vm.entries().at(1), pe("/b.ogg", "B", 20));
    }
    QVERIFY(f.invalidReasons.empty());
}

void TestPlaylistVM::moveRemoveClearDelegateAndNotifyUpdatedState() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));
    f.vm.add(pe("/b.ogg", "B", 20));
    f.vm.add(pe("/c.flac", "C", 30));

    // Reorder: moveEntry(2 → 0) yields [C, A, B].
    f.vm.moveEntry(2, 0);
    QCOMPARE(f.vm.entryCount(), size_t(3));
    if (f.vm.entryCount() == 3) {
        QCOMPARE(asQ(f.vm.entries().at(0).path), QString("/c.flac"));
        QCOMPARE(asQ(f.vm.entries().at(1).path), QString("/a.mp3"));
        QCOMPARE(asQ(f.vm.entries().at(2).path), QString("/b.ogg"));
    }
    // Remove the middle entry → [C, B].
    f.vm.removeAt(1);
    QCOMPARE(f.vm.entryCount(), size_t(2));
    if (f.vm.entryCount() == 2) {
        QCOMPARE(asQ(f.vm.entries().at(1).path), QString("/b.ogg"));
    }
    // Clear → empty; every delegate call notified exactly once.
    f.vm.clear();
    QCOMPARE(f.vm.entryCount(), size_t(0));
    QCOMPARE(f.order.size(), size_t(6));
    if (f.order.size() == 6) {
        QCOMPARE(asQ(f.order[3]), QString("changed:3"));
        QCOMPARE(asQ(f.order[4]), QString("changed:2"));
        QCOMPARE(asQ(f.order[5]), QString("changed:0"));
    }
    QVERIFY(f.invalidReasons.empty());
}

void TestPlaylistVM::domainNoopMutationsStillNotifyChangedWithStableState() {
    Fixture f;
    // Documented SH-603 decision: uniform change firing — domain no-ops
    // (out-of-range index, move from == to, clear of an empty playlist)
    // notify changed too, with the state observable as stable.
    f.vm.moveEntry(0, 1);  // empty playlist
    QCOMPARE(f.vm.entryCount(), size_t(0));
    QCOMPARE(lastEvent(f.order), QString("changed:0"));

    f.vm.add(pe("/a.mp3", "A", 10));
    f.vm.add(pe("/b.ogg", "B", 20));
    f.vm.moveEntry(5, 1);  // out of range
    f.vm.moveEntry(1, 1);  // from == to
    QCOMPARE(f.vm.entryCount(), size_t(2));
    if (f.vm.entryCount() == 2) {
        QCOMPARE(asQ(f.vm.entries().at(0).path), QString("/a.mp3"));
        QCOMPARE(asQ(f.vm.entries().at(1).path), QString("/b.ogg"));
    }
    f.vm.removeAt(9);  // out of range
    QCOMPARE(f.vm.entryCount(), size_t(2));
    f.vm.clear();  // second clear (empty again): still notifies
    QCOMPARE(f.order.size(), size_t(7));
    if (f.order.size() == 7) {
        QCOMPARE(asQ(f.order[1]), QString("changed:1"));
        // The four no-ops after the seed adds each notified with a stable
        // count: the state was already updated at callback time (ordering).
        QCOMPARE(asQ(f.order[2]), QString("changed:2"));
        QCOMPARE(asQ(f.order[3]), QString("changed:2"));
        QCOMPARE(asQ(f.order[4]), QString("changed:2"));
        QCOMPARE(asQ(f.order[5]), QString("changed:2"));
        QCOMPARE(asQ(f.order[6]), QString("changed:0"));
    }
    QVERIFY(f.invalidReasons.empty());
}

void TestPlaylistVM::deduplicateKeepsFirstOccurrenceThroughFacade() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "First", 10));
    f.vm.add(pe("/a.mp3", "Second", 10));  // same resolved path
    f.vm.add(pe("/c.flac", "C", 30));
    f.vm.deduplicate();
    // SH-304 semantics through the facade: keep the FIRST occurrence.
    QCOMPARE(f.vm.entryCount(), size_t(2));
    if (f.vm.entryCount() == 2) {
        QCOMPARE(f.vm.entries().at(0), pe("/a.mp3", "First", 10));
        QCOMPARE(f.vm.entries().at(1), pe("/c.flac", "C", 30));
    }
    // No duplicates left: a second dedupe is a domain no-op that still
    // notifies (uniform firing decision).
    f.vm.deduplicate();
    QCOMPARE(f.vm.entryCount(), size_t(2));
    QCOMPARE(f.order.size(), size_t(5));
    QCOMPARE(lastEvent(f.order), QString("changed:2"));
    QVERIFY(f.invalidReasons.empty());
}

void TestPlaylistVM::totalDurationPassesThroughFacade() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 180));
    f.vm.add(pe("/b.ogg", "B", 97));
    QCOMPARE(f.vm.totalDurationSeconds(), 277LL);
    f.vm.add(pe("/c.flac", "C", shanty::Track::kUnknownDuration));
    // Unknown duration in any entry flags the total (SH-304 contract).
    QCOMPARE(f.vm.totalDurationSeconds(),
             (long long)shanty::Track::kUnknownDuration);
    f.vm.removeAt(2);
    QCOMPARE(f.vm.totalDurationSeconds(), 277LL);
}

void TestPlaylistVM::importEditExportChainPreservesMetadataRoundTrip() {
    Fixture f;
    // 1) IMPORT: metadata + paths resolve at the boundary (SH-305/D3).
    shanty::ParseResult parsed;
    parsed.playlist.add(pe("song_a.mp3", "Song A", 180));
    parsed.playlist.add(pe("sub/song_b.ogg", "Song B", 97));
    parsed.playlist.add(pe("file:///music/song_c.flac", "Song C", 240));
    f.codec.nextParse = parsed;
    f.seedImportFile();
    f.vm.beginImport();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.vm.entryCount(), size_t(3));

    // 2) EDIT via the SH-304 facade. Duplicates removed before export.
    f.vm.add(pe(f.importQPath().section('/', 0, -2).toStdString() +
                    "/song_a.mp3",
                "Dup A", 180));
    f.vm.deduplicate();
    QCOMPARE(f.vm.entryCount(), size_t(3));
    if (f.vm.entryCount() == 3) {
        QCOMPARE(asQ(f.vm.entries().at(0).path),
                 f.importQPath().section('/', 0, -2) +
                     QString::fromLatin1("/song_a.mp3"));
        QCOMPARE(asQ(f.vm.entries().at(0).title), QString("Song A"));
    }

    // 3) EXPORT: the resolved playlist with persisted metadata round
    //    trips through the codec and lands in the file.
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Success;
    f.codec.nextSerialize.data = "#EXTM3U\n";
    f.vm.beginExport();
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.codec.serializeTargets.size(), size_t(1));
    if (f.codec.serializePlaylists.size() == 1) {
        const auto& es = f.codec.serializePlaylists.front().entries();
        QCOMPARE(es.at(0), pe(f.importQPath().section('/', 0, -2).toStdString() +
                                  "/song_a.mp3",
                              "Song A", 180));
        QCOMPARE(es.at(1),
                 pe(f.importQPath().section('/', 0, -2).toStdString() +
                        "/sub/song_b.ogg",
                    "Song B", 97));
        QCOMPARE(es.at(2), pe("/music/song_c.flac", "Song C", 240));
        QCOMPARE(f.codec.serializePlaylists.front().totalDurationSeconds(),
                 517LL);
    }
    QCOMPARE(witnessFileBytes(f.exportQPath(), f.codec.nextSerialize.data), true);
}

// ---- SH-807 (UX-11): playlist dirty guard ---------------------------------------
// UX-11: the single-playlist editor must not silently destroy work. The view
// model tracks a dirty state (edits since the last successful import/export)
// and guards beginImport: while dirty, the import first asks the user
// (AwaitingConfirm — the shell pushes the "Discard unsaved edits?" prompt);
// confirm proceeds to the file-picker flow, cancel keeps the edits. Export
// is deliberately NOT guarded (exporting cannot destroy the working state —
// it writes to a chosen target; a failed export keeps the edits dirty).

void TestPlaylistVM::freshViewModelIsNotDirty() {
    Fixture f;
    QCOMPARE(f.vm.isDirty(), false);
}

void TestPlaylistVM::dirtySetByMutationsThatChangeEntries() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));
    QCOMPARE(f.vm.isDirty(), true);  // add changed the entries

    f.vm.add(pe("/b.ogg", "B", 20));
    f.vm.moveEntry(1, 0);
    QCOMPARE(f.vm.isDirty(), true);  // reorder is an edit

    f.vm.removeAt(0);
    QCOMPARE(f.vm.isDirty(), true);

    f.vm.clear();
    QCOMPARE(f.vm.isDirty(), true);  // clearing work is destroying work
    QCOMPARE(f.vm.entryCount(), size_t(0));
}

void TestPlaylistVM::domainNoopMutationsDoNotSetDirty() {
    Fixture f;
    // Domain no-ops on a FRESH (clean) view model: the service fires
    // onPlaylistChanged but nothing mutated — the dirty state must NOT
    // treat them as edits.
    f.vm.moveEntry(0, 0);   // same index (empty anyway)
    f.vm.removeAt(0);       // out of range
    f.vm.clear();           // already empty
    QCOMPARE(f.vm.isDirty(), false);

    // One real edit flips it; further no-ops leave it untouched.
    f.vm.add(pe("/a.mp3", "A", 10));
    QCOMPARE(f.vm.isDirty(), true);
    f.vm.moveEntry(0, 0);
    f.vm.removeAt(5);
    QCOMPARE(f.vm.isDirty(), true);  // unchanged by the no-ops
}

void TestPlaylistVM::deduplicateSetsDirtyOnlyWhenEntriesChange() {
    Fixture f;
    // Duplicates present: dedupe changes the entries → an edit.
    f.vm.add(pe("/a.mp3", "A", 10));
    f.vm.add(pe("/a.mp3", "A", 10));
    f.vm.deduplicate();
    QCOMPARE(f.vm.entryCount(), size_t(1));
    QCOMPARE(f.vm.isDirty(), true);

    // Unique entries: dedupe is a domain no-op → the dirty state is NOT
    // touched (a fresh fixture is clean, dedupe keeps it clean).
    Fixture g;
    g.vm.add(pe("/a.mp3", "A", 10));
    g.vm.add(pe("/b.ogg", "B", 20));
    g.vm.deduplicate();
    QCOMPARE(g.vm.entryCount(), size_t(2));
    QCOMPARE(g.vm.isDirty(), false);
}

void TestPlaylistVM::beginImportWhileDirtyAwaitsConfirmAndConfirmDiscardProceeds() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));

    // Dirty → the import first asks (new AwaitingConfirm state).
    f.vm.beginImport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingConfirm);
    QCOMPARE(f.vm.isDirty(), true);  // nothing discarded yet
    QCOMPARE(f.vm.entryCount(), size_t(1));

    // Confirm (discard) → proceeds to the file-picker flow (pending op
    // preserved as Import — the picked file is imported normally).
    f.codec.nextParse.playlist.add(pe("/imported.mp3", "Imp", 5));
    f.seedImportFile();
    f.vm.confirmDiscard();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.vm.entryCount(), size_t(1));
    QCOMPARE(asQ(f.vm.entries().front().path), QString("/imported.mp3"));
}

void TestPlaylistVM::confirmDiscardOutsideAwaitingConfirmIsRejectedNoop() {
    Fixture f;
    f.vm.confirmDiscard();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    QCOMPARE(lastReason(f.invalidReasons),
             QString("confirmDiscard: not awaiting confirm"));

    // Also rejected from AwaitingFile (the discard prompt is up only in
    // AwaitingConfirm; a confirm there would silently skip the guard).
    f.vm.beginImport();
    f.vm.confirmDiscard();
    QCOMPARE(lastReason(f.invalidReasons),
             QString("confirmDiscard: not awaiting confirm"));
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
}

void TestPlaylistVM::cancelFromAwaitingConfirmKeepsEditsAndDirty() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));
    f.vm.add(pe("/b.ogg", "B", 20));
    f.vm.beginImport();  // dirty → AwaitingConfirm
    f.vm.cancelFlow();

    // Cancel = "keep the edits": back to Idle, entries intact, still dirty.
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Idle);
    QCOMPARE(f.vm.isDirty(), true);
    QCOMPARE(f.vm.entryCount(), size_t(2));
    QCOMPARE(f.invalidReasons.size(), size_t(0));  // a valid cancel: no toast
}

void TestPlaylistVM::beginExportWhileDirtyIsNotGuarded() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));

    // Exporting cannot destroy the working state (it writes a chosen
    // target) — a dirty playlist goes straight to the file-picker flow.
    f.vm.beginExport();
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::AwaitingFile);
}

void TestPlaylistVM::successfulImportClearsDirtyFailedImportKeepsIt() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));

    // Success path: the imported entries become the new baseline → clean.
    f.codec.nextParse.playlist.add(pe("/imported.mp3", "Imp", 5));
    f.seedImportFile();
    f.vm.beginImport();  // dirty → AwaitingConfirm
    f.vm.confirmDiscard();
    f.vm.confirmImport(f.importPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.vm.isDirty(), false);

    // Failure path: the import replaced nothing (the edits are intact) —
    // the working state stays dirty.
    f.codec.nextParse = shanty::ParseResult{};  // default: parse errors empty?
    // Note: a FAILED import is driven by an unreadable file (fails before
    // the codec) — reuse the documented unreadable-file path.
    f.vm.add(pe("/b.ogg", "B", 20));
    QCOMPARE(f.vm.isDirty(), true);
    f.vm.beginImport();  // dirty → AwaitingConfirm
    f.vm.confirmDiscard();
    f.vm.confirmImport("/nonexistent/missing.m3u");
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(f.vm.isDirty(), true);  // the edits survive a failed import
    QCOMPARE(f.vm.entryCount(), size_t(2));
}

void TestPlaylistVM::successfulExportClearsDirtyFailedExportKeepsIt() {
    Fixture f;
    f.vm.add(pe("/a.mp3", "A", 10));

    // Success: the work is saved → clean.
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Success;
    f.codec.nextSerialize.data = "#EXTM3U\n";
    f.vm.beginExport();
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Done);
    QCOMPARE(f.vm.isDirty(), false);

    // Failure: the work is NOT saved → still dirty.
    f.vm.add(pe("/b.ogg", "B", 20));
    f.codec.nextSerialize.status = shanty::SerializeResult::Status::Error;
    f.codec.nextSerialize.errorMessage = "disk full";
    f.vm.beginExport();
    f.vm.confirmExport(f.exportPath());
    QCOMPARE(f.vm.flowState(), PlaylistFlowState::Failed);
    QCOMPARE(f.vm.isDirty(), true);
}

QTEST_MAIN(TestPlaylistVM)
#include "tst_playlist_vm.moc"
