// SH-602 Phase A: library view-model suite (pure C++ facade over
// LibraryService + ILibraryStore; decisions V1..V8 documented in
// presentation/viewmodels/LibraryViewModel.h). Every delegation is asserted
// exactly: call counts AND args (one store.save per effective mutation, an
// exact-one scan passthrough with the current config, reload through the
// store). Copyright 2026 Shanty contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "FakeLibraryStore.h"
#include "RecordingScanner.h"
#include "domain/LibraryConfig.h"
#include "services/LibraryService.h"
#include "viewmodels/LibraryViewModel.h"

using shanty::LibraryConfig;
using shanty::LibraryViewModel;

namespace {

std::vector<std::string> toVector(std::initializer_list<const char*> paths) {
    return std::vector<std::string>(paths.begin(), paths.end());
}

QStringList qList(const std::vector<std::string>& paths) {
    QStringList out;
    for (const std::string& p : paths) out << QString::fromStdString(p);
    return out;
}

QList<int> indices(const std::vector<std::size_t>& selection) {
    return QList<int>(selection.begin(), selection.end());
}

LibraryConfig configOf(std::vector<std::string> folders,
                       std::vector<std::string> whitelist = {},
                       std::vector<std::string> blacklist = {}) {
    LibraryConfig config;
    config.folders = std::move(folders);
    config.whitelist = std::move(whitelist);
    config.blacklist = std::move(blacklist);
    return config;
}

/// Suite fixture: scripted store + scripted scanner; the view-model is
/// constructed AFTER the store is scripted (its constructor already
/// loads through the store — V6).
struct Vm {
    FakeLibraryStore store;
    RecordingScanner scanner;
    shanty::LibraryService service;
    LibraryViewModel vm;

    explicit Vm(LibraryConfig loaded = LibraryConfig(),
                std::vector<std::string> scanned = {})
        : store(std::move(loaded)),
          scanner(std::move(scanned)),
          service(scanner),
          vm(store, service) {}
};

}  // namespace

class TestLibraryVm : public QObject {
    Q_OBJECT

private slots:
    // ---- V6 config lifecycle ------------------------------------------------
    void constructionLoadsConfigExactlyOnceThroughStore();
    void reloadReplacesWorkingConfigThroughStore();
    void scanAndReloadNeverSave();

    // ---- folder management (V1/V2/V4) ----------------------------------------
    void addFolderAppendsSavesOnceAndNotifies();
    void addFolderRejectsEmptyPath();
    void addFolderRejectsDuplicatePath();
    void removeFolderErasesSavesOnceAndNotifies();
    void removeFolderOfUnknownPathIsIdempotentNoOp();

    // ---- whitelist/blacklist (V1/V2/V3/V4) -----------------------------------
    void filterListMutationsSaveOnceEach();
    void filterListAddsAreSetSemantics();
    void filterListRemovalOfMissingEntryIsNoOp();
    void filterListRejectsEmptyPath();

    // ---- listing passthrough (V5/V7) -------------------------------------------
    void scanDelegatesExactlyOnceWithCurrentConfig();
    void scanIsPurePassthroughNoRefiltering();
    void scanStoresListingAndResetsSelection();

    // ---- selection (V7) ---------------------------------------------------------
    void selectAllSelectionAndPathsAscending();
    void clearSelectionEmptiesBothViews();
    void setSelectedTogglesSingleIndices();
    void setSelectedOutOfRangeIsRejected();
    void reloadClearsSelection();

    // ---- SH-809 (UX-8): row context-menu command catalogue (red-first) -------
    void rowCommandsOfferPlayAndAddToQueue();
    void rowCommandsOnOutOfRangeIndexAreEmpty();
    void rowCommandsAreIndependentOfSelectionState();
};

// ---- V6 config lifecycle ------------------------------------------------------

void TestLibraryVm::constructionLoadsConfigExactlyOnceThroughStore() {
    Vm v{configOf(toVector({"/music"}))};
    QCOMPARE(v.store.loadCount_, 1);  // exactly one load at construction
    QCOMPARE(qList(v.vm.config().folders), QStringList{"/music"});
    // Construction is NOT a mutation (V6): nothing saved, nothing evented.
    QCOMPARE(v.store.saveCount_, 0);
}

void TestLibraryVm::reloadReplacesWorkingConfigThroughStore() {
    Vm v{configOf(toVector({"/music"}))};
    QCOMPARE(v.store.loadCount_, 1);

    int configChanged = 0;
    QStringList invalid;
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };

    v.vm.addFolder("/music/extra");
    QCOMPARE(v.store.saveCount_, 1);  // the add was persisted immediately

    // The store now carries a different config; reload() re-loads through it.
    v.store.loaded_ = configOf(toVector({"/media"}),
                               toVector({"/media/k.ogg"}),
                               toVector({"/media/j.mp3"}));
    v.vm.reload();

    QCOMPARE(v.store.loadCount_, 2);  // one load per reload, through the store
    QCOMPARE(qList(v.vm.config().folders), QStringList{"/media"});
    QCOMPARE(qList(v.vm.config().whitelist), QStringList{"/media/k.ogg"});
    QCOMPARE(qList(v.vm.config().blacklist), QStringList{"/media/j.mp3"});
    // reload() is not a mutation (V4/V6): no further save, and NOT an event.
    QCOMPARE(v.store.saveCount_, 1);
    QCOMPARE(configChanged, 1);  // only the addFolder event fired so far
    QCOMPARE(invalid, QStringList());
}

void TestLibraryVm::scanAndReloadNeverSave() {
    Vm v{configOf(toVector({"/music"}))};
    v.vm.scan();
    v.vm.reload();
    // Removing a blacklisted entry that is not listed is an idempotent no-op.
    v.vm.removeBlacklistEntry("/nope");
    // Listing + reload are never persisted (V4): only loadCount grows.
    QCOMPARE(v.store.saveCount_, 0);
    QCOMPARE(v.store.loadCount_, 2);  // construction + reload
    QCOMPARE(v.scanner.callCount_, 1);  // exact-one scan passthrough
}

// ---- folder management (V1/V2/V4) ---------------------------------------------

void TestLibraryVm::addFolderAppendsSavesOnceAndNotifies() {
    Vm v{configOf(toVector({"/music"}))};
    int configChanged = 0;
    QStringList invalid;
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };

    v.vm.addFolder("/media");

    QCOMPARE(qList(v.vm.config().folders), (QStringList{"/music", "/media"}));
    QVERIFY(v.store.lastSaved_ == v.vm.config());  // the new state was saved
    QCOMPARE(v.store.saveCount_, 1);  // V4: exactly one save per mutation
    QCOMPARE(configChanged, 1);       // V8: one event per persisted mutation
    QCOMPARE(invalid, QStringList());
}

void TestLibraryVm::addFolderRejectsEmptyPath() {
    Vm v{configOf(toVector({"/music"}))};
    QStringList invalid;
    int configChanged = 0;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };

    v.vm.addFolder("");

    // V1: NOT silently ignored — explicit rejection, stable reason string.
    QCOMPARE(invalid, QStringList{"addFolder: empty path"});
    QCOMPARE(qList(v.vm.config().folders), QStringList{"/music"});
    QCOMPARE(v.store.saveCount_, 0);  // rejected mutations never persist
    QCOMPARE(configChanged, 0);
}

void TestLibraryVm::addFolderRejectsDuplicatePath() {
    Vm v{configOf(toVector({"/music", "/media"}))};
    QStringList invalid;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.onConfigChanged = []() { QFAIL("no event expected"); };

    v.vm.addFolder("/music");

    QCOMPARE(invalid, QStringList{"addFolder: already managed"});
    QCOMPARE(qList(v.vm.config().folders), (QStringList{"/music", "/media"}));
    QCOMPARE(v.store.saveCount_, 0);
}

void TestLibraryVm::removeFolderErasesSavesOnceAndNotifies() {
    Vm v{configOf(toVector({"/a", "/b", "/c"}))};
    int configChanged = 0;
    QStringList invalid;
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };

    v.vm.removeFolder("/b");

    QCOMPARE(qList(v.vm.config().folders), (QStringList{"/a", "/c"}));
    QVERIFY(v.store.lastSaved_ == v.vm.config());
    QCOMPARE(v.store.saveCount_, 1);
    QCOMPARE(configChanged, 1);
    QCOMPARE(invalid, QStringList());
}

void TestLibraryVm::removeFolderOfUnknownPathIsIdempotentNoOp() {
    Vm v{configOf(toVector({"/a"}))};
    QStringList invalid;
    int configChanged = 0;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };

    v.vm.removeFolder("/zzz");

    // V2: removals are idempotent — harmless no-op, no rejection channel.
    QCOMPARE(invalid, QStringList());
    QCOMPARE(configChanged, 0);
    QCOMPARE(qList(v.vm.config().folders), QStringList{"/a"});
    QCOMPARE(v.store.saveCount_, 0);
}

// ---- whitelist/blacklist (V1/V2/V3/V4) -----------------------------------------

void TestLibraryVm::filterListMutationsSaveOnceEach() {
    Vm v{LibraryConfig()};
    QStringList invalid;
    int configChanged = 0;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };

    v.vm.addWhitelistEntry("/w");
    v.vm.addBlacklistEntry("/b");
    v.vm.removeWhitelistEntry("/w");
    v.vm.removeBlacklistEntry("/b");

    QCOMPARE(qList(v.vm.config().whitelist), QStringList());
    QCOMPARE(qList(v.vm.config().blacklist), QStringList());
    QCOMPARE(v.store.saveCount_, 4);       // V4: one save per effective change
    QVERIFY(v.store.lastSaved_ == v.vm.config());  // the final state was saved
    QCOMPARE(configChanged, 4);
    QCOMPARE(invalid, QStringList());
}

void TestLibraryVm::filterListAddsAreSetSemantics() {
    Vm v{LibraryConfig()};
    QStringList invalid;
    int configChanged = 0;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.onConfigChanged = [&configChanged]() { ++configChanged; };

    v.vm.addWhitelistEntry("/w");
    v.vm.addWhitelistEntry("/w");
    v.vm.addBlacklistEntry("/b");
    v.vm.addBlacklistEntry("/b");

    // V3: set semantics — no duplicates, no error channel; the idempotent
    // (second) adds are silent, the effective ones notify AND persist.
    QCOMPARE(qList(v.vm.config().whitelist), QStringList{"/w"});
    QCOMPARE(qList(v.vm.config().blacklist), QStringList{"/b"});
    QCOMPARE(v.store.saveCount_, 2);   // one per EFFECTIVE add only
    QCOMPARE(configChanged, 2);        // ditto for the event (V8)
    QCOMPARE(invalid, QStringList());
}

void TestLibraryVm::filterListRemovalOfMissingEntryIsNoOp() {
    Vm v{configOf({}, toVector({"/w"}), toVector({"/b"}))};
    QStringList invalid;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };

    v.vm.removeWhitelistEntry("/nope");
    v.vm.removeBlacklistEntry("/nope");

    // V2: idempotent no-op for all three lists.
    QCOMPARE(invalid, QStringList());
    QCOMPARE(qList(v.vm.config().whitelist), QStringList{"/w"});
    QCOMPARE(qList(v.vm.config().blacklist), QStringList{"/b"});
    QCOMPARE(v.store.saveCount_, 0);
}

void TestLibraryVm::filterListRejectsEmptyPath() {
    Vm v{LibraryConfig()};
    QStringList invalid;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.onConfigChanged = []() { QFAIL("rejected add must not notify"); };

    v.vm.addWhitelistEntry("");
    v.vm.addBlacklistEntry("");

    QCOMPARE(invalid, (QStringList{"addWhitelistEntry: empty path",
                                   "addBlacklistEntry: empty path"}));
    QCOMPARE(qList(v.vm.config().whitelist), QStringList());
    QCOMPARE(qList(v.vm.config().blacklist), QStringList());
    QCOMPARE(v.store.saveCount_, 0);
}

// ---- listing passthrough (V5/V7) -------------------------------------------------

void TestLibraryVm::scanDelegatesExactlyOnceWithCurrentConfig() {
    Vm v{configOf(toVector({"/music", "/media"})),
         toVector({"/music/a.mp3", "/media/k.ogg"})};

    const QStringList result = qList(v.vm.scan());

    QCOMPARE(result, (QStringList{"/music/a.mp3", "/media/k.ogg"}));
    QCOMPARE(v.scanner.callCount_, 1);  // exact-one delegation (V7)
    // The CURRENT working config reaches the service, verbatim.
    QCOMPARE(static_cast<int>(v.scanner.scannedFolders_.size()), 1);
    QCOMPARE(qList(v.scanner.scannedFolders_.front()),
             (QStringList{"/music", "/media"}));
    // Listing/delegating is never persisted (V4).
    QCOMPARE(v.store.saveCount_, 0);
}

void TestLibraryVm::scanIsPurePassthroughNoRefiltering() {
    // The VM only edits config + triggers scans (V5); the composition rule
    // (blacklist beats whitelist, supported extensions, managed folders)
    // stays entirely inside LibraryService. Proof: the VM listing is
    // IDENTICAL to calling LibraryService directly with the same config.
    Vm v{configOf(toVector({"/music"}), toVector({"/music/a"}),
                  toVector({"/music/skip"})),
         toVector({"/music/a.mp3", "/music/keep.flac", "/music/note.txt",
                   "/nowhere/x.mp3"})};

    const QStringList result = qList(v.vm.scan());
    QCOMPARE(v.scanner.callCount_, 1);

    const shanty::LibraryService direct(v.scanner);
    const QStringList expected = qList(direct.scan(v.vm.config()));

    QCOMPARE(result, expected);
    // Composition DID apply exactly once (through the delegated service):
    // blacklisted and unsupported/out-of-tree candidates are gone, and the
    // non-empty whitelist restricts to its entry — "/music/keep.flac" is
    // NOT whitelisted ("keep" does not match "/music/a"), so the SERVICE
    // dropped it (V5: the composition decision belongs to SH-202, and the
    // VM produces the identical result as a direct service call).
    QCOMPARE(result, (QStringList{"/music/a.mp3"}));
}

void TestLibraryVm::scanStoresListingAndResetsSelection() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac"})};

    // Empty listing until the first scan (V7).
    QCOMPARE(qList(v.vm.tracks()), QStringList());

    // selectAll on an empty listing selects nothing (correct state, V7).
    v.vm.selectAll();
    QCOMPARE(indices(v.vm.selection()), QList<int>());

    QStringList listing = qList(v.vm.scan());
    QCOMPARE(qList(v.vm.tracks()),
             (QStringList{"/music/a.mp3", "/music/b.flac"}));
    QCOMPARE(listing, qList(v.vm.tracks()));  // scan() result == listing
    QCOMPARE(indices(v.vm.selection()), QList<int>());  // nothing was selected

    // A selection made against THIS listing must not survive a re-scan.
    v.vm.setSelected(1, true);
    QCOMPARE(indices(v.vm.selection()), (QList<int>{1}));

    listing = qList(v.vm.scan());
    QCOMPARE(listing, qList(v.vm.tracks()));    // listing re-fetched
    QCOMPARE(indices(v.vm.selection()), QList<int>());  // old indices reset
}

// ---- selection (V7) ----------------------------------------------------------------

void TestLibraryVm::selectAllSelectionAndPathsAscending() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/c.ogg", "/music/a.mp3", "/music/b.flac"})};
    v.vm.scan();

    v.vm.selectAll();

    QCOMPARE(indices(v.vm.selection()), (QList<int>{0, 1, 2}));
    QCOMPARE(qList(v.vm.selectionPaths()),
             (QStringList{"/music/c.ogg", "/music/a.mp3", "/music/b.flac"}));
}

void TestLibraryVm::clearSelectionEmptiesBothViews() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac"})};
    v.vm.scan();
    v.vm.selectAll();

    v.vm.clearSelection();

    QCOMPARE(indices(v.vm.selection()), QList<int>());
    QCOMPARE(qList(v.vm.selectionPaths()), QStringList());
    // Selection is view state only — never persisted (V4).
    QCOMPARE(v.store.saveCount_, 0);
}

void TestLibraryVm::setSelectedTogglesSingleIndices() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac", "/music/c.ogg"})};
    QStringList invalid;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.scan();

    v.vm.setSelected(1, true);
    QCOMPARE(indices(v.vm.selection()), (QList<int>{1}));
    v.vm.setSelected(2, true);
    QCOMPARE(indices(v.vm.selection()), (QList<int>{1, 2}));  // ascending
    v.vm.setSelected(1, false);
    QCOMPARE(indices(v.vm.selection()), (QList<int>{2}));
    QCOMPARE(qList(v.vm.selectionPaths()), QStringList{"/music/c.ogg"});
    QCOMPARE(invalid, QStringList());  // in-range toggling is not a rejection
}

void TestLibraryVm::setSelectedOutOfRangeIsRejected() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac"})};
    QStringList invalid;
    v.vm.onInvalid = [&invalid](const char* reason) {
        invalid << QString::fromUtf8(reason);
    };
    v.vm.scan();

    v.vm.setSelected(2, true);
    v.vm.setSelected(0, true);
    v.vm.setSelected(2, false);  // also rejected beyond the listing (and
                                 // beyond it there is nothing to unselect)

    QCOMPARE(invalid, (QStringList{"setSelected: index out of range",
                                   "setSelected: index out of range"}));
    QCOMPARE(indices(v.vm.selection()), (QList<int>{0}));
    QCOMPARE(qList(v.vm.selectionPaths()), QStringList{"/music/a.mp3"});
}

void TestLibraryVm::reloadClearsSelection() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac"})};
    v.vm.scan();
    v.vm.setSelected(1, true);

    v.vm.reload();

    // V6: indices refer to a listing that may no longer match the new
    // folders, so a reload clears the selection; the listing itself is
    // retained until the next scan() request.
    QCOMPARE(indices(v.vm.selection()), QList<int>());
    QCOMPARE(qList(v.vm.selectionPaths()), QStringList());
    QCOMPARE(qList(v.vm.tracks()), (QStringList{"/music/a.mp3", "/music/b.flac"}));
}

// ---- SH-809 (UX-8): row context-menu command catalogue ------------------------
// The pressAndHold menu content is a VIEW-MODEL decision (host-tested): a
// library track row offers Play (the SH-802 queue-replace bridge with just
// this path — plays the track NOW) and AddToQueue (the non-destructive
// queue append). The catalogue is a pure function of the index validity;
// selection state does not filter it (the selection buttons stay the
// discoverable surface; the menu is the per-row shortcut).

void TestLibraryVm::rowCommandsOfferPlayAndAddToQueue() {
    // The listing must pass the SH-202 filter (managed folders) — the
    // scanner result alone is not the listing.
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac"})};
    v.vm.scan();

    const auto commands = v.vm.rowCommands(0);
    QCOMPARE(commands.size(), size_t(2));
    if (commands.size() == 2) {
        QCOMPARE(commands[0], LibraryViewModel::RowCommand::Play);
        QCOMPARE(commands[1], LibraryViewModel::RowCommand::AddToQueue);
    }
}

void TestLibraryVm::rowCommandsOnOutOfRangeIndexAreEmpty() {
    Vm v{configOf(toVector({"/music"})), toVector({"/music/a.mp3"})};
    v.vm.scan();

    // No row → no menu: the empty catalogue is the QML "do not open" signal.
    QCOMPARE(v.vm.rowCommands(1).empty(), true);
    QCOMPARE(v.vm.rowCommands(99).empty(), true);
}

void TestLibraryVm::rowCommandsAreIndependentOfSelectionState() {
    Vm v{configOf(toVector({"/music"})),
         toVector({"/music/a.mp3", "/music/b.flac"})};
    v.vm.scan();
    v.vm.setSelected(0, true);  // selection active

    // The catalogue does not flip with the selection state.
    const auto commands = v.vm.rowCommands(1);
    QCOMPARE(commands.size(), size_t(2));
    if (commands.size() == 2) {
        QCOMPARE(commands[0], LibraryViewModel::RowCommand::Play);
        QCOMPARE(commands[1], LibraryViewModel::RowCommand::AddToQueue);
    }
}

QTEST_MAIN(TestLibraryVm)
#include "tst_library_vm.moc"
