// SH-501 Phase A: EqualizerProfile validation + named profile persistence
// via SettingsService (with the trusted InMemorySettingsStore fake).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "services/SettingsService.h"
#include "fakes/InMemorySettingsStore.h"

using shanty::EqualizerProfile;
using shanty::SettingsService;

bool operator==(const EqualizerProfile& a, const EqualizerProfile& b) {
    return a.name() == b.name() && a.gains() == b.gains();
}

class TestEqualizerProfile : public QObject {
    Q_OBJECT

private slots:
    void flatProfileIsAllZero();
    void flatDefaultHasTenBands();
    void isValidEnforcesGainRange();
    void saveTooLoudProfileIsRejected();
    void saveLoadRoundTrip();
    void saveOverwritesSameName();
    void deleteProfileRemovesIt();
    void loadWhenEmptyReturnsNothing();
};

void TestEqualizerProfile::flatProfileIsAllZero() {
    const EqualizerProfile flat = EqualizerProfile::flat("flat", 6);
    QCOMPARE(flat.gains(), std::vector<double>(6, 0.0));
    QVERIFY(flat.isValid());
}

void TestEqualizerProfile::flatDefaultHasTenBands() {
    const EqualizerProfile flat = EqualizerProfile::flat();
    QCOMPARE(int(flat.gains().size()), EqualizerProfile::kDefaultBandCount);
    QVERIFY(flat.name().empty());
}

void TestEqualizerProfile::isValidEnforcesGainRange() {
    const double limit = EqualizerProfile::kMaxAbsGainDb;
    const EqualizerProfile withBounds = EqualizerProfile::make(
        "p", {+limit, -limit});
    QVERIFY(withBounds.isValid());

    const EqualizerProfile tooLoud = EqualizerProfile::make(
        "p", {limit + 1.0, 0.0});
    QVERIFY(!tooLoud.isValid());

    const EqualizerProfile wrongCount = EqualizerProfile::flat("p", 0);
    QVERIFY(!wrongCount.isValid());
}

void TestEqualizerProfile::saveTooLoudProfileIsRejected() {
    InMemorySettingsStore store;
    SettingsService service(store);
    // +15 dB exceeds kMaxAbsGainDb: rejected, nothing written.
    const EqualizerProfile profile = EqualizerProfile::make(
        "bad", {EqualizerProfile::kMaxAbsGainDb + 3.0,
                EqualizerProfile::kMaxAbsGainDb + 3.0});
    QVERIFY(!service.saveProfile(profile));
    QCOMPARE(store.writeCount, 0);
}

void TestEqualizerProfile::saveLoadRoundTrip() {
    InMemorySettingsStore store;
    SettingsService service(store);

    const EqualizerProfile profile = EqualizerProfile::flat("rock", 10);
    QVERIFY(service.saveProfile(profile));

    const std::vector<EqualizerProfile> loaded = service.loadProfiles();
    QCOMPARE(int(loaded.size()), 1);
    QVERIFY(loaded[0] == profile);
}

void TestEqualizerProfile::saveOverwritesSameName() {
    InMemorySettingsStore store;
    SettingsService service(store);

    QVERIFY(service.saveProfile(EqualizerProfile::flat("rock", 10)));
    QVERIFY(service.saveProfile(EqualizerProfile::flat("rock", 6)));

    const std::vector<EqualizerProfile> loaded = service.loadProfiles();
    QCOMPARE(int(loaded.size()), 1);
    QCOMPARE(int(loaded[0].gains().size()), 6);
}

void TestEqualizerProfile::deleteProfileRemovesIt() {
    InMemorySettingsStore store;
    SettingsService service(store);
    QVERIFY(service.saveProfile(EqualizerProfile::flat("jazz", 10)));
    QVERIFY(service.deleteProfile("jazz"));
    QVERIFY(service.loadProfiles().empty());
    QVERIFY(!service.deleteProfile("missing"));
}

void TestEqualizerProfile::loadWhenEmptyReturnsNothing() {
    InMemorySettingsStore store;
    SettingsService service(store);
    QVERIFY(service.loadProfiles().empty());
}

QTEST_MAIN(TestEqualizerProfile)
#include "tst_equalizer_profile.moc"
