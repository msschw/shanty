// SH-502 Phase A: EffectChainService — applying/bypassing EqualizerProfiles
// on a recording FakeEffectChain, with transport independence asserted on a
// minimal FakeAudioEngine. Decisions under test are recorded in
// core/services/EffectChainService.h.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "services/EffectChainService.h"
#include "fakes/FakeAudioEngine.h"
#include "fakes/FakeEffectChain.h"

using shanty::EffectChainService;
using shanty::EqualizerProfile;
using shanty::IEffectChain;

namespace {

bool allZeros(const std::vector<double>& gains) {
    for (double gain : gains) {
        if (gain != 0.0) {
            return false;
        }
    }
    return true;
}

}  // namespace

class TestEffectChain : public QObject {
    Q_OBJECT

private slots:
    void applyProfileIssuesExactlyProfileGains();
    void applyFlatProfileSendsAllZeroGains();
    void applyExtremeGainsPassThroughUnchanged();
    void applyInvalidProfileRejectedWithoutTouchingChain();
    void bandCountMismatchRejectedWithoutTouchingChain();
    void resetCallsBypassAndLeavesFlatZeroGains();
    void resetOnFreshServiceStillBypasses();
    void applyAgainAfterResetSendsGainsAgain();
    void profileSwitchWhilePlayingKeepsTransportRunning();
    void resetWhilePlayingKeepsTransportRunning();
};

void TestEffectChain::applyProfileIssuesExactlyProfileGains() {
    FakeEffectChain chain;
    EffectChainService service(chain, 10);

    const std::vector<double> tenBandGains = {+3.5, -1.0, 0.0, +6.25, -8.0,
                                          +2.0, -0.5, +1.0, +4.0, -3.75};
    const EqualizerProfile profile =
        EqualizerProfile::make("smile", tenBandGains);

    QCOMPARE(service.applyProfile(profile), true);
    QCOMPARE(chain.applyGainsCalls, 1);
    QVERIFY(chain.lastGains == profile.gains());  // exact gains, in band order
}

void TestEffectChain::applyFlatProfileSendsAllZeroGains() {
    FakeEffectChain chain;
    EffectChainService service(chain, 6);

    const EqualizerProfile flat = EqualizerProfile::flat("zeroed", 6);
    QCOMPARE(service.applyProfile(flat), true);
    QCOMPARE(chain.applyGainsCalls, 1);
    QCOMPARE(int(chain.lastGains.size()), 6);
    QVERIFY(allZeros(chain.lastGains));
}

void TestEffectChain::applyExtremeGainsPassThroughUnchanged() {
    FakeEffectChain chain;
    EffectChainService service(chain, 4);

    // Applying any numerically different gain is a misconfiguration, not a
    // real fix. TODO(SH-502 Phase B) preserve this contract.
    const double limit = EqualizerProfile::kMaxAbsGainDb;
    const EqualizerProfile loud = EqualizerProfile::make(
        "limits", {-limit, +limit, -limit, +limit});

    QCOMPARE(service.applyProfile(loud), true);
    QVERIFY(chain.lastGains == loud.gains());
}

void TestEffectChain::applyInvalidProfileRejectedWithoutTouchingChain() {
    FakeEffectChain chain;
    EffectChainService service(chain, 10);

    // Out-of-range gain: invalid profile.
    const EqualizerProfile bad = EqualizerProfile::make(
        "bad", {0.0, EqualizerProfile::kMaxAbsGainDb + 1.0, 0.0, 0.0, 0.0});
    QVERIFY(!bad.isValid());

    QCOMPARE(service.applyProfile(bad), false);
    QCOMPARE(chain.applyGainsCalls, 0);
    QCOMPARE(chain.bypassCalls, 0);
    QVERIFY(chain.lastGains.empty());
}

void TestEffectChain::bandCountMismatchRejectedWithoutTouchingChain() {
    // Decision 1: no truncation/zero-padding. A 6-band profile into a
    // 10-band chain is rejected and the chain keeps its previous state.
    FakeEffectChain chain;
    EffectChainService service(chain, 10);

    const EqualizerProfile narrow = EqualizerProfile::make(
        "narrow", {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
    QCOMPARE(int(narrow.gains().size()), 6);

    QCOMPARE(service.applyProfile(narrow), false);
    QCOMPARE(chain.applyGainsCalls, 0);
    QCOMPARE(chain.bypassCalls, 0);

    // The previously applied profile (10 bands) is still in effect.
    const EqualizerProfile wide = EqualizerProfile::make(
        "wide", std::vector<double>(10, 0.0));
    QCOMPARE(service.applyProfile(wide), true);
    QCOMPARE(chain.applyGainsCalls, 1);
    QCOMPARE(int(chain.lastGains.size()), 10);
}

void TestEffectChain::resetCallsBypassAndLeavesFlatZeroGains() {
    FakeEffectChain chain;
    EffectChainService service(chain, 10);

    const EqualizerProfile profile = EqualizerProfile::make(
        "pulse", {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0});
    QCOMPARE(service.applyProfile(profile), true);

    service.reset();
    QCOMPARE(chain.bypassCalls, 1);  // decision 3, mechanism one
    QCOMPARE(int(chain.lastGains.size()), 10);
    QVERIFY(allZeros(chain.lastGains));  // decision 3, mechanism two
}

void TestEffectChain::resetOnFreshServiceStillBypasses() {
    FakeEffectChain chain;
    EffectChainService service(chain, 10);

    service.reset();
    QCOMPARE(chain.bypassCalls, 1);
    QCOMPARE(int(chain.lastGains.size()), 10);
    QVERIFY(allZeros(chain.lastGains));
}

void TestEffectChain::applyAgainAfterResetSendsGainsAgain() {
    FakeEffectChain chain;
    EffectChainService service(chain, 5);

    const std::vector<double> gains = {+1.0, -2.0, +3.0, -4.0, +5.0};
    const EqualizerProfile profile = EqualizerProfile::make("again", gains);

    service.reset();
    QCOMPARE(service.applyProfile(profile), true);
    QCOMPARE(chain.bypassCalls, 1);           // reset() still counted once
    QCOMPARE(chain.applyGainsCalls, 2);       // reset's flats + the profile
    QVERIFY(chain.lastGains == gains);
}

void TestEffectChain::profileSwitchWhilePlayingKeepsTransportRunning() {
    // Decision 4: application is transport-independent. Uses the shared
    // SH-404 FakeAudioEngine (controllable posSeconds/durSeconds/state).
    FakeEffectChain chain;
    FakeAudioEngine engine;
    engine.play("file.flac");
    engine.posSeconds = 42;
    engine.durSeconds = 180;

    EffectChainService service(chain, 10, &engine);
    const EqualizerProfile first = EqualizerProfile::flat("first", 10);
    QCOMPARE(service.applyProfile(first), true);

    const EqualizerProfile second =
        EqualizerProfile::make("second", std::vector<double>(10, -0.5));

    const int playCount = int(engine.playCalls.size());
    QCOMPARE(service.applyProfile(second), true);

    QCOMPARE(engine.state(), shanty::IAudioEngine::State::Playing);
    QCOMPARE(engine.positionSeconds(), 42L);   // position NOT reset
    QCOMPARE(engine.durationSeconds(), 180L);
    QCOMPARE(int(engine.playCalls.size()), playCount);  // no replay
    QCOMPARE(engine.stopCalls, 0);             // never stopped
    QCOMPARE(engine.pauseCalls, 0);
    QVERIFY(!engine.playCalls.empty());
    QCOMPARE(engine.playCalls.back(), std::string("file.flac"));
    QVERIFY(chain.lastGains == second.gains());
}

void TestEffectChain::resetWhilePlayingKeepsTransportRunning() {
    FakeEffectChain chain;
    FakeAudioEngine engine;
    engine.play("song.ogg");
    engine.posSeconds = 7;

    EffectChainService service(chain, 10, &engine);
    service.reset();

    QCOMPARE(engine.state(), shanty::IAudioEngine::State::Playing);
    QCOMPARE(engine.positionSeconds(), 7L);
    QCOMPARE(engine.stopCalls, 0);
    QCOMPARE(engine.pauseCalls, 0);
    QCOMPARE(chain.bypassCalls, 1);
    QVERIFY(allZeros(chain.lastGains));
}

QTEST_MAIN(TestEffectChain)
#include "tst_effect_chain.moc"
