// SH-502 Phase B: EffectChainService implementation.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Decision record (also in EffectChainService.h):
//  1. Band-count rule — applyProfile() rejects profiles whose band count
//     differs from the chain band count supplied at construction (the
//     engine adapter owns the chain and knows its EQ band count; the port
//     has no band-count query). No truncation / zero-padding is performed;
//     mismatched gains must never reach real EQ hardware.
//  2. Validity rule — profiles failing EqualizerProfile::isValid() (gains
//     outside [-12, +12] dB, zero bands) are rejected before the chain is
//     touched. Rejections happen in the same order (validity first), so a
//     mismatched AND invalid profile is rejected for the validity reason.
//  3. Bypass semantics — reset() is the "bypass / apply-flat" request and
//     exercises both mechanisms, belt-and-braces: chain.bypass() first (so
//     the backend drops processing), then all-zero gains over the chain
//     band count via applyGains() (so the last-known gains are unambiguously
//     flat even on backends that ignore bypass()). Zero gains are a valid
//     domain state, so this can never push the chain out of range.
//  4. Transport independence — neither applyProfile() nor reset() calls the
//     optional transport reference; it exists purely for observation by
//     hosts/tests of this service. A profile switch mid-playback therefore
//     never restarts, pauses, or resets the current entry.
//
// Callbacks vs exceptions: the API is value-based (bool/void), no exceptions
// are thrown or caught; rejections are reported by the return value.
//
#include "services/EffectChainService.h"

namespace shanty {

EffectChainService::EffectChainService(IEffectChain& chain, int chainBandCount,
                                       IAudioEngine* transport)
    : chain_(chain),
      chainBandCount_(chainBandCount),
      transport_(transport) {}

bool EffectChainService::applyProfile(const EqualizerProfile& profile) {
    // Decision 2: reject invalid profiles before the chain is touched.
    if (!profile.isValid()) {
        return false;
    }

    // Decision 1: reject band-count mismatches instead of guessing a mapping.
    if (static_cast<int>(profile.gains().size()) != chainBandCount_) {
        return false;
    }

    // Decision 4: transport stays untouched — only the chain is driven.
    chain_.applyGains(profile.gains());
    return true;
}

void EffectChainService::reset() {
    // Decision 3: bypass first, then explicit flat gains.
    chain_.bypass();
    chain_.applyGains(std::vector<double>(chainBandCount_ < 0 ? 0
                                                             : chainBandCount_,
                                         0.0));
}

}  // namespace shanty
