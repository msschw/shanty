// SH-902 Phase A stub: neutral implementation so the tests/eq_mapping
// cases FAIL BY ASSERTION (not by crash) — red-first, per the standing
// Phase-A rule. The real log-domain mapping lands in Phase B.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "EqualizerBandMapping.h"

namespace shanty {

const double DeviceEqualizerBands::kFrequenciesHz[DeviceEqualizerBands::kBandCount] = {
    31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};

std::vector<double> mapGainsToDeviceBands(
    const std::vector<std::pair<double, double>>& frequencyGainPointsHzDb) {
    // Neutral stub: always flat at the device band count.
    (void)frequencyGainPointsHzDb;
    return std::vector<double>(DeviceEqualizerBands::kBandCount, 0.0);
}

}  // namespace shanty
