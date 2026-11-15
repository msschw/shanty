// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later

// SH-605 Phase A: SettingsViewModel — the settings-page logic layer.
//
// Contracts under test (decision records in
// presentation/viewmodels/SettingsViewModel.h):
//   - profile CRUD delegation to SettingsService (SH-501), asserted on a
//     recording fake ISettingsStore by call counts and exact key/value args;
//   - current-profile selection persisted under "equalizer/current"
//     (view-model-owned setting, decision 2);
//   - "apply current profile" assembling the SH-502 call: an
//     EffectChainService is constructed by the view model with the chain
//     sized to the current profile's band count (decision 3), exact gains
//     land on the fake chain, and every refusal is a callback + a no-op on
//     chain and store;
//   - AutoEq import flow delegating verbatim to IAutoEqReader and
//     persisting a ProfileSource::Imported profile; reader errors and
//     non-persistable (out-of-range) parse results surface as structured
//     values, never exceptions (decision 5);
//   - "changes survive reload": a view model reconstructed over the same
//     fake store re-reads the persisted profiles and selection (decision 6).
//
// Baseline expectations note: many tests seed profiles directly through the
// REAL SettingsService over the fake store (the trusted, SH-501-green
// persistence path), then count store-write deltas from that baseline.

#include <QtTest>

#include <string>
#include <vector>

#include "fakes/FakeEffectChain.h"

#include "FakeAutoEqReader.h"
#include "FakePersistingLibraryStore.h"
#include "FakeSettingsStore.h"
#include "presentation/viewmodels/SettingsViewModel.h"
#include "services/SettingsService.h"

#include "core/services/LibraryService.h"
#include "presentation/viewmodels/LibraryViewModel.h"

#include "fakes/FakeScanner.h"

using shanty::AutoEqError;
using shanty::AutoEqParseResult;
using shanty::EqualizerProfile;
using shanty::ProfileSource;
using shanty::SettingsService;
using shanty::SettingsViewModel;
using shanty::LibraryConfig;

namespace {

constexpr const char* kProfilesKey = "equalizer/profiles";

/// Recorded observation of the view model's callbacks, wired fresh per test.
struct Hooks {
    int profilesChanged = 0;
    int selectionChanged = 0;
    std::vector<std::string> rejected;
    std::vector<std::string> imported;
    std::vector<std::vector<AutoEqError>> importRejections;

    void attach(SettingsViewModel& vm) {
        vm.onProfilesChanged = [this]() { ++profilesChanged; };
        vm.onSelectionChanged = [this]() { ++selectionChanged; };
        vm.onRejected = [this](const char* reason) {
            rejected.push_back(reason);
        };
        vm.onImported = [this](const std::string& name) {
            imported.push_back(name);
        };
        vm.onImportRejected = [this](
                                  const std::vector<AutoEqError>& errors) {
            importRejections.push_back(errors);
        };
    }
};

/// Seeds a profile through the real SH-501 service (trusted persistence).
void seedProfile(FakeSettingsStore& store, const std::string& name,
                 const std::vector<double>& gains,
                 ProfileSource source = ProfileSource::Manual) {
    SettingsService service(store);
    service.saveProfile(EqualizerProfile::makeWithSource(name, gains, source));
}

bool containsWrite(const FakeSettingsStore& store, const std::string& key,
                   const std::string& needle) {
    for (const auto& call : store.writeCalls) {
        if (call.first == key && call.second.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

class TestSettingsVm : public QObject {
    Q_OBJECT

private slots:
    void constructorLoadsProfilesFromStore();
    void createPersistsViaServiceAndAutoSelects();
    void createWithEmptyNameRejected();
    void createWithExistingNameRejected();
    void renameMovesProfileAndKeepsSelection();
    void renameToExistingNameRejected();
    void deleteClearsSelectionOfDeletedProfile();
    void deleteUnknownProfileRejected();
    void selectPersistsAndNotifies();
    void selectUnknownProfileRejected();
    void applySendsExactGainsToChainWithoutStoreWrites();
    void applyWithoutSelectionRejected();
    void applyIsSizedToTheProfileBandCount();
    void resetBypassesAndFlattensSelectedProfile();
    void resetWithoutSelectionFlattensDefaultBandCount();
    void importDelegatesToReaderAndPersistsImportedProfile();
    void importInvalidFileForwardsStructuredErrors();
    void importOutOfRangeGainsNotPersisted();
    void changesSurviveReloadThroughSameStore();
    // SH-1004 (UX-15): library-config commands on the settings side
    void libraryConfigAddFolderPersistsToTheLibraryStore();
    void libraryConfigFolderValidationVerbatim();
    void libraryConfigFilterValidationAndSetSemantics();
    void libraryConfigRemovalsAreIdempotentNoOps();
    void libraryConfigCrossVisibilityBothWaysThroughOneStore();
};

void TestSettingsVm::constructorLoadsProfilesFromStore() {
    FakeSettingsStore store;
    seedProfile(store, "loud", {+2.0, -3.0});
    seedProfile(store, "mild", {+0.25, -0.25, 1.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;

    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    const int readsBaseline = 0;
    QCOMPARE(int(vm.profiles().size()), 2);  // re-read from the store
    QCOMPARE(vm.profiles()[0].name(), std::string("loud"));
    QVERIFY(vm.profiles()[0].gains() == std::vector<double>({+2.0, -3.0}));
    QCOMPARE(vm.profiles()[1].name(), std::string("mild"));
    QVERIFY(vm.profiles()[1].gains() ==
            std::vector<double>({+0.25, -0.25, 1.0}));
    QVERIFY(vm.currentProfileName().empty());  // nothing persisted as current
    QVERIFY(store.readAllCount > readsBaseline);  // went through the store
    QCOMPARE(hooks.profilesChanged, 0);           // load is quiet
}

void TestSettingsVm::createPersistsViaServiceAndAutoSelects() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.createProfile("fresh", 3);

    // Delegation went through the store: constructor reload + one read
    // inside SettingsService::saveProfile + one post-save loadProfiles
    // re-read; the exact profile write is asserted by key and value below.
    QVERIFY(store.readAllCount >= 3);
    // Decision 1: exactly one profile write, via SettingsService.
    QCOMPARE(int(store.writeCalls.size()), 2);
    QCOMPARE(store.writeCalls[0].first, std::string(kProfilesKey));
    QVERIFY(store.writeCalls[0].second.find("fresh") != std::string::npos);
    QCOMPARE(store.writeCalls[0].second.find("fresh"), std::size_t(0));
    // Decision 2: the new profile is auto-selected and persisted as current.
    QCOMPARE(store.writeCalls[1].first, std::string("equalizer/current"));
    QCOMPARE(store.writeCalls[1].second, std::string("fresh"));
    QCOMPARE(vm.currentProfileName(), std::string("fresh"));
    QCOMPARE(hooks.profilesChanged, 1);
    QCOMPARE(hooks.selectionChanged, 1);
    QVERIFY(hooks.rejected.empty());

    vm.createProfile("other");  // default 10 bands survives a reload too
    QCOMPARE(int(vm.profiles().size()), 2);
    QCOMPARE(vm.profiles()[0].name(), std::string("fresh"));
    QVERIFY(vm.profiles()[0].gains() ==
            std::vector<double>({0.0, 0.0, 0.0}));
    QCOMPARE(vm.profiles()[1].name(), std::string("other"));
    QCOMPARE(int(vm.profiles()[1].gains().size()), 10);
}

void TestSettingsVm::createWithEmptyNameRejected() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.createProfile("");

    QCOMPARE(hooks.rejected, std::vector<std::string>{"create: empty profile name"});
    QVERIFY(store.writeCalls.empty());  // complete no-op: nothing persisted
    QCOMPARE(int(vm.profiles().size()), 0);
    QCOMPARE(hooks.selectionChanged, 0);
}

void TestSettingsVm::createWithExistingNameRejected() {
    FakeSettingsStore store;
    seedProfile(store, "taken", {1.0, 2.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    const int writesBefore = int(store.writeCalls.size());
    vm.createProfile("taken");

    QCOMPARE(hooks.rejected, std::vector<std::string>{"create: profile name already exists"});
    QCOMPARE(int(store.writeCalls.size()), writesBefore);  // no write, no replace
    QCOMPARE(int(vm.profiles().size()), 1);
    QVERIFY(vm.profiles()[0].gains() == std::vector<double>({1.0, 2.0}));
}

void TestSettingsVm::renameMovesProfileAndKeepsSelection() {
    FakeSettingsStore store;
    seedProfile(store, "draft", {+1.5, -2.5});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);
    vm.selectProfile("draft");
    QCOMPARE(store.writeCalls.back().first, std::string("equalizer/current"));
    const int writesBeforeRename = int(store.writeCalls.size());

    vm.renameProfile("draft", "final");

    // Decision 1: composite of saveProfile(new) + deleteProfile(old); the
    // store saw the profiles value rewritten and the old record is gone.
    QCOMPARE(store.writeCalls.back().first, std::string("equalizer/current"));
    QCOMPARE(store.writeCalls.back().second, std::string("final"));
    QVERIFY(containsWrite(store, kProfilesKey, "final"));
    bool oldNameStillStored = false;
    for (int i = int(store.writeCalls.size()) - 1; i >= writesBeforeRename; --i) {
        const auto& call = store.writeCalls[i];
        if (call.first == kProfilesKey) {
            // Check the LAST post-rename profiles write: it is the state
            // another reader would observe (the intermediate save write
            // still holds the old record that deleteProfile then removed).
            oldNameStillStored =
                call.second.find("draft") != std::string::npos;
            break;
        }
    }
    QVERIFY(!oldNameStillStored);  // the last profiles write dropped "draft"
    QCOMPARE(int(vm.profiles().size()), 1);
    QCOMPARE(vm.profiles()[0].name(), std::string("final"));
    QVERIFY(vm.profiles()[0].gains() == std::vector<double>({+1.5, -2.5}));
    QCOMPARE(vm.profiles()[0].source(), ProfileSource::Manual);  // preserved
    QCOMPARE(vm.currentProfileName(), std::string("final"));
    QCOMPARE(hooks.profilesChanged, 1);
    QCOMPARE(hooks.selectionChanged, 2);  // initial select + repoint
    QVERIFY(hooks.rejected.empty());
}

void TestSettingsVm::renameToExistingNameRejected() {
    FakeSettingsStore store;
    seedProfile(store, "a", {1.0});
    seedProfile(store, "b", {2.0, 2.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    const int writesBefore = int(store.writeCalls.size());
    vm.renameProfile("a", "b");

    QCOMPARE(hooks.rejected, std::vector<std::string>{"rename: name already exists"});
    QCOMPARE(int(store.writeCalls.size()), writesBefore);  // nothing persisted
    QCOMPARE(int(vm.profiles().size()), 2);  // both profiles still there
}

void TestSettingsVm::deleteClearsSelectionOfDeletedProfile() {
    FakeSettingsStore store;
    seedProfile(store, "first", {0.5});
    seedProfile(store, "second", {1.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);
    vm.selectProfile("second");
    const int writesAfterSelect = int(store.writeCalls.size());

    vm.deleteProfile("second");

    // Decision 1: one SettingsService delete (profiles value rewritten
    // without the deleted record).
    QCOMPARE(int(store.writeCalls.size()), writesAfterSelect + 1);
    QVERIFY(containsWrite(store, kProfilesKey, "first"));
    QCOMPARE(int(vm.profiles().size()), 1);
    QCOMPARE(vm.profiles()[0].name(), std::string("first"));
    // Decision 2: the deleted profile was current → selection cleared.
    QCOMPARE(vm.currentProfileName(), std::string(""));
    QCOMPARE(int(store.removeCalls.size()), 1);
    QCOMPARE(store.removeCalls.back(), std::string("equalizer/current"));
    QCOMPARE(hooks.profilesChanged, 1);
    QCOMPARE(hooks.selectionChanged, 2);  // select + clear
}

void TestSettingsVm::deleteUnknownProfileRejected() {
    FakeSettingsStore store;
    seedProfile(store, "real", {0.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    const int writesBefore = int(store.writeCalls.size());
    vm.deleteProfile("ghost");

    QCOMPARE(hooks.rejected, std::vector<std::string>{"delete: unknown profile"});
    QCOMPARE(int(store.writeCalls.size()), writesBefore);
    QCOMPARE(int(store.removeCalls.size()), 0);
    QCOMPARE(int(vm.profiles().size()), 1);
}

void TestSettingsVm::selectPersistsAndNotifies() {
    FakeSettingsStore store;
    seedProfile(store, "one", {0.0});
    seedProfile(store, "two", {1.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.selectProfile("two");

    // Decision 2: selection persisted by the view model's own key.
    QCOMPARE(store.writeCalls.back().first, std::string("equalizer/current"));
    QCOMPARE(store.writeCalls.back().second, std::string("two"));
    QCOMPARE(vm.currentProfileName(), std::string("two"));
    QCOMPARE(hooks.selectionChanged, 1);
    QCOMPARE(hooks.rejected.empty() ? 0 : 1, 0);

    // Redundant re-select of the current name: no write, no hook.
    const int writesBefore = int(store.writeCalls.size());
    vm.selectProfile("two");
    QCOMPARE(int(store.writeCalls.size()), writesBefore);
    QCOMPARE(hooks.selectionChanged, 1);
}

void TestSettingsVm::selectUnknownProfileRejected() {
    FakeSettingsStore store;
    seedProfile(store, "one", {0.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    const int writesBefore = int(store.writeCalls.size());  // seed writes
    vm.selectProfile("ghost");

    QCOMPARE(hooks.rejected, std::vector<std::string>{"select: unknown profile"});
    QCOMPARE(int(store.writeCalls.size()), writesBefore);  // nothing written
    QCOMPARE(vm.currentProfileName(), std::string(""));
    QCOMPARE(hooks.selectionChanged, 0);
}

void TestSettingsVm::applySendsExactGainsToChainWithoutStoreWrites() {
    FakeSettingsStore store;
    seedProfile(store, "smile", {+3.5, -1.0, 0.0, +6.25, -8.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    vm.selectProfile("smile");

    const int writesBefore = int(store.writeCalls.size());
    vm.applyCurrentProfile();

    // Decision 3 (SH-502 call assembly): exact profile gains in band order.
    QCOMPARE(chain.applyGainsCalls, 1);
    QCOMPARE(chain.bypassCalls, 0);
    QVERIFY(chain.lastGains ==
            std::vector<double>({+3.5, -1.0, 0.0, +6.25, -8.0}));
    // Applying is a runtime-only chain action: no persistence happens.
    QCOMPARE(int(store.writeCalls.size()), writesBefore);
}

void TestSettingsVm::applyWithoutSelectionRejected() {
    FakeSettingsStore store;
    seedProfile(store, "idle", {1.0, 2.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.applyCurrentProfile();

    QCOMPARE(hooks.rejected, std::vector<std::string>{"apply: no profile selected"});
    QCOMPARE(chain.applyGainsCalls, 0);  // chain untouched
    QCOMPARE(chain.bypassCalls, 0);
    QVERIFY(chain.lastGains.empty());
}

void TestSettingsVm::applyIsSizedToTheProfileBandCount() {
    // Decision 3: the glue owns the band count — a 3-band profile applies
    // cleanly because the view model constructs EffectChainService with the
    // profile's own band count (no fixed-chain validation here, no
    // truncation/zero-padding either: the exact gains pass through).
    FakeSettingsStore store;
    seedProfile(store, "narrow", {1.5, -2.5, 3.5});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    vm.selectProfile("narrow");

    vm.applyCurrentProfile();

    QCOMPARE(chain.applyGainsCalls, 1);
    QCOMPARE(int(chain.lastGains.size()), 3);
    QVERIFY(chain.lastGains == std::vector<double>({1.5, -2.5, 3.5}));
}

void TestSettingsVm::resetBypassesAndFlattensSelectedProfile() {
    // Decision 7: bypass first, then zero gains sized to the selected
    // profile's band count (SH-502 decision 3's double mechanism).
    FakeSettingsStore store;
    seedProfile(store, "colored", {+4.0, -4.0, +1.0});
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    vm.selectProfile("colored");

    vm.resetEqualizer();

    QCOMPARE(chain.bypassCalls, 1);
    QCOMPARE(chain.applyGainsCalls, 1);
    QCOMPARE(int(chain.lastGains.size()), 3);
    QVERIFY(chain.lastGains == std::vector<double>({0.0, 0.0, 0.0}));
}

void TestSettingsVm::resetWithoutSelectionFlattensDefaultBandCount() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);

    vm.resetEqualizer();

    QCOMPARE(chain.bypassCalls, 1);
    QCOMPARE(chain.applyGainsCalls, 1);
    QCOMPARE(int(chain.lastGains.size()),
             shanty::EqualizerProfile::kDefaultBandCount);
    bool allZero = true;
    for (double gain : chain.lastGains) {
        allZero = allZero && gain == 0.0;
    }
    QVERIFY(allZero);
}

void TestSettingsVm::importDelegatesToReaderAndPersistsImportedProfile() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    // Canned SH-503 result: parsed gains, source tracking done by the
    // reader (Imported by contract).
    reader.next.profile = EqualizerProfile::makeWithSource(
        std::string(), {-0.2, +1.5, -3.2}, ProfileSource::Imported);
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.importAutoEqProfile("HD 800 Grid", "GraphicEQ: 20 -0.2; 21 1.5; 20000 -3.2",
                           "autoeq/GraphicEQ.txt");

    // Decision 5: the reader saw the content and path verbatim, once.
    QCOMPARE(reader.readCalls, 1);
    QCOMPARE(reader.readData.back(),
             std::string("GraphicEQ: 20 -0.2; 21 1.5; 20000 -3.2"));
    QCOMPARE(reader.readFilePaths.back(), std::string("autoeq/GraphicEQ.txt"));
    // The parsed gains were persisted under the caller-supplied name.
    QVERIFY(containsWrite(store, kProfilesKey, "HD 800 Grid"));
    QCOMPARE(int(vm.profiles().size()), 1);
    QCOMPARE(vm.profiles()[0].name(), std::string("HD 800 Grid"));
    QVERIFY(vm.profiles()[0].gains() ==
            std::vector<double>({-0.2, +1.5, -3.2}));
    QCOMPARE(vm.profiles()[0].source(), ProfileSource::Manual);
    // ^ SH-501 wire-format limitation, documented (not this ticket's core/
    //   file): SettingsService's wire record is "name⇥gains" only, so
    //   parseProfiles() rebuilds profiles with the default source Manual
    //   after ANY reload. The import DID stamp Imported at save time
    //   (decisions 5's makeWithSource call); the flag survives the in-app
    //   life of the process only until the next full list reload. Fixing
    //   the wire format belongs to the SH-501 owner.
    QCOMPARE(vm.currentProfileName(), std::string(""));  // no auto-select
    QCOMPARE(hooks.imported, std::vector<std::string>{"HD 800 Grid"});
    QVERIFY(hooks.importRejections.empty());
}

void TestSettingsVm::importInvalidFileForwardsStructuredErrors() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    AutoEqError bad{AutoEqError::Code::UnrecognizedFormat,
                    "content is not one of the documented AutoEq graphic-EQ "
                    "spellings",
                    3};
    reader.next.errors.push_back(bad);
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.importAutoEqProfile("broken", "garbage text", "autoeq/bad.txt");

    QCOMPARE(reader.readCalls, 1);
    QCOMPARE(hooks.importRejections.size(), std::size_t(1));
    QCOMPARE(hooks.importRejections[0].size(), std::size_t(1));
    QCOMPARE(hooks.importRejections[0][0].code, bad.code);
    QCOMPARE(hooks.importRejections[0][0].message, bad.message);
    QCOMPARE(hooks.importRejections[0][0].line, bad.line);
    QVERIFY(store.writeCalls.empty());  // nothing persisted
    QVERIFY(vm.profiles().empty());
    QCOMPARE(hooks.imported.empty() ? 0 : 1, 0);
}

void TestSettingsVm::importOutOfRangeGainsNotPersisted() {
    // Decision 5: parse-ok but gains beyond ±12 dB — saveProfile refuses an
    // invalid profile (SH-501, before touching the store) and the view
    // model surfaces ONE structured error naming the cause.
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    reader.next.profile = EqualizerProfile::makeWithSource(
        std::string(), {20.0, -2.0}, ProfileSource::Imported);
    QVERIFY(!reader.next.profile.isValid());
    QVERIFY(reader.next.ok());  // parse itself is clean
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    vm.importAutoEqProfile("too Loud", "GraphicEQ: 20 20.0; 21 -2.0");

    QCOMPARE(reader.readCalls, 1);
    QCOMPARE(hooks.importRejections.size(), std::size_t(1));
    QCOMPARE(hooks.importRejections[0].size(), std::size_t(1));
    QVERIFY(hooks.importRejections[0][0].message.find("12") !=
            std::string::npos);  // states the ±12 dB cause
    QVERIFY(store.writeCalls.empty());  // the store was never touched
    QVERIFY(vm.profiles().empty());
    QCOMPARE(hooks.imported.empty() ? 0 : 1, 0);
}

void TestSettingsVm::changesSurviveReloadThroughSameStore() {
    // Decision 6: profiles + selection persist; the chain state does not —
    // it is reproduced by a fresh apply.
    FakeSettingsStore store;
    seedProfile(store, "seeded", {+2.0, -2.0});
    FakeEffectChain chainA;
    FakeAutoEqReader readerA;
    readerA.next.profile = EqualizerProfile::makeWithSource(
        std::string(), {+0.5, -0.5, +1.0}, ProfileSource::Imported);
    FakePersistingLibraryStore storeLibraryStore;
    SettingsViewModel vmA(store, chainA, readerA, storeLibraryStore);
    vmA.createProfile("manual mixed", 2);
    vmA.importAutoEqProfile("imported 3", "GraphicEQ: 20 0.5; 21 -0.5; 22 1.0");
    vmA.selectProfile("seeded");
    vmA.applyCurrentProfile();
    QVERIFY(chainA.lastGains == std::vector<double>({+2.0, -2.0}));

    // "Reload": a fresh view model over the SAME store re-reads state.
    FakeEffectChain chainB;
    FakeAutoEqReader readerB;
    FakePersistingLibraryStore libraryStoreB;
    SettingsViewModel vmB(store, chainB, readerB, libraryStoreB);

    QCOMPARE(int(vmB.profiles().size()), 3);
    QCOMPARE(vmB.profiles()[0].name(), std::string("seeded"));
    QCOMPARE(vmB.profiles()[1].name(), std::string("manual mixed"));
    QVERIFY(vmB.profiles()[1].gains() == std::vector<double>({0.0, 0.0}));
    QCOMPARE(vmB.profiles()[2].name(), std::string("imported 3"));
    // Source resets to Manual across the reload (SH-501 wire format does
    // not carry ProfileSource — see the longer note in
    // importDelegatesToReaderAndPersistsImportedProfile).
    QCOMPARE(vmB.profiles()[2].source(), ProfileSource::Manual);
    QCOMPARE(vmB.currentProfileName(), std::string("seeded"));

    vmB.applyCurrentProfile();
    QVERIFY(chainB.lastGains == std::vector<double>({+2.0, -2.0}));

    const int readsBefore = readerB.readCalls;
    vmB.selectProfile("imported 3");
    vmB.applyCurrentProfile();
    QCOMPARE(readerB.readCalls, readsBefore);  // applying needs no import
    QVERIFY(chainB.lastGains ==
            std::vector<double>({+0.5, -0.5, +1.0}));
}

// ---- SH-1004 (UX-15): library-config commands on the settings side ----------
// Owner direction (2026-11-14): library management lives ONLY in Settings.
// The settings view model edits the SAME LibraryConfig persistence the
// library side uses (single write path, one shared editing implementation —
// the LibraryViewModel rejection catalogue strings verbatim), and the two
// view models see each other's writes through their reload paths.

void TestSettingsVm::libraryConfigAddFolderPersistsToTheLibraryStore() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);
    // The store may start pre-populated (a config from a previous session);
    // a fresh settings view model must LOAD it, not start empty.
    shanty::LibraryConfig seeded;
    seeded.folders = {"/music"};
    libraryStore.save(seeded);
    const int savesBefore = libraryStore.saveCount_;

    vm.addFolder("/podcasts");

    // No rejection, one persisted mutation, and the working config shows it.
    QCOMPARE(int(hooks.rejected.size()), 0);
    QCOMPARE(int(vm.libraryConfig().folders.size()), 2);
    QVERIFY(vm.libraryConfig().folders[1] == "/podcasts");
    QCOMPARE(libraryStore.saveCount_, savesBefore + 1);
    // The mutation is DURABLE: a fresh load of the store returns it.
    QVERIFY(shanty::LibraryConfig(vm.libraryConfig()) == libraryStore.load());
}

void TestSettingsVm::libraryConfigFolderValidationVerbatim() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);
    const int savesBefore = libraryStore.saveCount_;

    vm.addFolder("");
    // The LibraryViewModel catalogue string, verbatim, through the settings
    // rejection channel (the existing toast mapping).
    QCOMPARE(int(hooks.rejected.size()), 1);
    QVERIFY(hooks.rejected[0] == "addFolder: empty path");
    QCOMPARE(libraryStore.saveCount_, savesBefore);  // rejected = no save

    vm.addFolder("/music");
    vm.addFolder("/music");
    QCOMPARE(int(hooks.rejected.size()), 2);
    QVERIFY(hooks.rejected[1] == "addFolder: already managed");
    QCOMPARE(int(vm.libraryConfig().folders.size()), 1);
    QCOMPARE(libraryStore.saveCount_, savesBefore + 1);
}

void TestSettingsVm::libraryConfigFilterValidationAndSetSemantics() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);
    const int savesBefore = libraryStore.saveCount_;

    // Empty entries are rejected with the catalogue strings, verbatim.
    vm.addWhitelistEntry("");
    vm.addBlacklistEntry("");
    QCOMPARE(int(hooks.rejected.size()), 2);
    QVERIFY(hooks.rejected[0] == "addWhitelistEntry: empty path");
    QVERIFY(hooks.rejected[1] == "addBlacklistEntry: empty path");
    QCOMPARE(libraryStore.saveCount_, savesBefore);

    // Set semantics: a duplicate filter entry is a silent idempotent no-op
    // (no rejection, no save) — the V3 rule, shared verbatim.
    vm.addWhitelistEntry("/music/fav");
    vm.addWhitelistEntry("/music/fav");
    vm.addBlacklistEntry("/music/skip");
    vm.addBlacklistEntry("/music/skip");
    QCOMPARE(int(hooks.rejected.size()), 2);  // no new rejections
    QCOMPARE(int(vm.libraryConfig().whitelist.size()), 1);
    QCOMPARE(int(vm.libraryConfig().blacklist.size()), 1);
    QCOMPARE(libraryStore.saveCount_, savesBefore + 2);  // one save per add
}

void TestSettingsVm::libraryConfigRemovalsAreIdempotentNoOps() {
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);
    Hooks hooks;
    hooks.attach(vm);

    // Removing a missing entry is a silent no-op across all three lists.
    vm.removeFolder("/nope");
    vm.removeWhitelistEntry("/nope");
    vm.removeBlacklistEntry("/nope");
    QCOMPARE(int(hooks.rejected.size()), 0);
    QCOMPARE(libraryStore.saveCount_, 0);

    // Adding then removing a folder persists both transitions.
    vm.addFolder("/music");
    vm.removeFolder("/music");
    QCOMPARE(libraryStore.saveCount_, 2);
    QCOMPARE(int(vm.libraryConfig().folders.size()), 0);
    // And a config that round-trips through reload keeps the removal.
    vm.reloadLibraryConfig();
    QCOMPARE(int(vm.libraryConfig().folders.size()), 0);
}

void TestSettingsVm::libraryConfigCrossVisibilityBothWaysThroughOneStore() {
    // THE single-write-path contract: two view models (settings side and
    // library side) over ONE store see each other's writes after a reload.
    FakeSettingsStore store;
    FakeEffectChain chain;
    FakeAutoEqReader reader;
    FakePersistingLibraryStore libraryStore;
    SettingsViewModel vm(store, chain, reader, libraryStore);

    // A library view model over the SAME store (a fake scanner suffices —
    // the cross-visibility contract is about the CONFIG, not the listing).
    FakeScanner scanner({});
    shanty::LibraryService libraryService(scanner);
    shanty::LibraryViewModel libraryVm(libraryStore, libraryService);

    // Settings side writes → library side sees it after reload().
    vm.addFolder("/music");
    libraryVm.reload();
    QCOMPARE(int(libraryVm.config().folders.size()), 1);
    QVERIFY(libraryVm.config().folders[0] == "/music");

    // Library side writes → settings side sees it after reloadLibraryConfig().
    libraryVm.addBlacklistEntry("/music/skip");
    vm.reloadLibraryConfig();
    QCOMPARE(int(vm.libraryConfig().blacklist.size()), 1);
    QVERIFY(vm.libraryConfig().blacklist[0] == "/music/skip");
    // ... and the settings side's earlier write is still there for it.
    QCOMPARE(int(vm.libraryConfig().folders.size()), 1);
}

QTEST_MAIN(TestSettingsVm)
#include "tst_settings_vm.moc"
