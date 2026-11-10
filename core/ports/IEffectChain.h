// SH-502 port: equalizer/effects application to the audio output.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <vector>

namespace shanty {

class IEffectChain {
public:
    virtual ~IEffectChain() = default;

    /// Applies band gains (dB) in band order.
    virtual void applyGains(const std::vector<double>& gainsDb) = 0;
    /// Resets all bands to 0 dB (bypass).
    virtual void bypass() = 0;
};

}  // namespace shanty
