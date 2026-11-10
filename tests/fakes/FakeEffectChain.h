// Shared test double for SH-502. Copyright 2026 Shanty contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <vector>

#include "core/ports/IEffectChain.h"

/// A recording fake of the SH-502 effect-chain port: it stores the last
/// gains it received (plus the full call history) and counts bypass calls.
/// The fake is trusted; the service under test (EffectChainService) is what
/// Phase A exercises.
class FakeEffectChain : public shanty::IEffectChain {
public:
    void applyGains(const std::vector<double>& gainsDb) override {
        lastGains = gainsDb;
        history.push_back(gainsDb);
        ++applyGainsCalls;
    }

    void bypass() override { ++bypassCalls; }

    std::vector<double> lastGains;
    std::vector<std::vector<double>> history;
    int applyGainsCalls = 0;
    int bypassCalls = 0;
};
