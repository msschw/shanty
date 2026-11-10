// SH-502: EffectChainService — wires EqualizerProfile domain data onto the
// IEffectChain port, transport-independently.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../ports/IAudioEngine.h"
#include "../ports/IEffectChain.h"
#include "domain/EqualizerProfile.h"

// Decisions recorded for this ticket (SH-502, each exercised by
// tests/effect_chain/tst_effect_chain.cpp):
//
// 1. Band-count rule: the IEffectChain port has no band-count query, so the
//    chain's band count is supplied at construction by whoever wires the
//    chain (the engine adapter owns the chain and knows its EQ band count).
//    applyProfile() REJECTS a profile whose band count differs from the
//    chain's (returns false, chain untouched, no partial state) rather than
//    truncating or zero-padding gains — feeding a mismatched band mapping to
//    a real EQ is a misapplication of the user's intent, and guessing a
//    mapping would be silent data corruption.
// 2. Validity rule: profiles failing EqualizerProfile::isValid() (out-of-range
//    gains, zero bands) are likewise rejected before the chain is touched.
// 3. Bypass semantics: reset() is the "bypass / apply-flat" request. Both
//    mechanisms are exercised, belt-and-braces: chain.bypass() is invoked
//    first so the backend drops any processing, then explicit all-zero gains
//    sized to the chain band count are applied via applyGains(), so the
//    last-known gains view is unambiguously flat even on backends that
//    ignore bypass(). All-zero gains within ±12 dB are themselves a valid
//    profile, so this never pushes the chain outside its domain.
// 4. Transport independence: profile application and reset() never drive the
//    transport (no play/pause/stop, no position change). An optional passive
//    IAudioEngine reference may be attached at construction so hosts of the
//    service can assert on the shared engine; the service never calls the
//    transport. CONSEQUENCE: a profile switch mid-playback does not restart
//    or reset the current entry.

namespace shanty {

class EffectChainService {
public:
    /// @param chainBandCount the EQ band count the wired chain expects.
    /// @param transport optional passive reference (see decision 4).
    EffectChainService(IEffectChain& chain, int chainBandCount,
                       IAudioEngine* transport = nullptr);

    /// Applies the profile's gains to the chain in band order.
    /// Returns false (chain untouched) when the profile is invalid or its
    /// band count differs from the chain's (decisions 1-2).
    bool applyProfile(const EqualizerProfile& profile);

    /// Bypass request: chain.bypass() + flat zero gains (decision 3).
    void reset();

    int chainBandCount() const { return chainBandCount_; }

private:
    IEffectChain& chain_;
    int chainBandCount_;
    IAudioEngine* transport_;  // passive; never driven (decision 4)
};

}  // namespace shanty
