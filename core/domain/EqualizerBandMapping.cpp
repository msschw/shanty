// SH-902 Phase B: the log-domain mapping implementation. Contract:
// core/domain/EqualizerBandMapping.h (curve model + clamping + the
// replaces-SH-502-rejection decision).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "EqualizerBandMapping.h"

#include <algorithm>
#include <cmath>

namespace shanty {

const double DeviceEqualizerBands::kFrequenciesHz[DeviceEqualizerBands::kBandCount] = {
    31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};

std::vector<double> mapGainsToDeviceBands(
    const std::vector<std::pair<double, double>>& frequencyGainPointsHzDb) {
    std::vector<double> result(DeviceEqualizerBands::kBandCount, 0.0);

    // Sort ascending by frequency (order-independent input). A stable
    // sort keeps the LAST of duplicate frequencies last, so the dedupe
    // below implements "last wins" (defensive; the AutoEq reader rejects
    // duplicates).
    std::vector<std::pair<double, double>> points(frequencyGainPointsHzDb);
    std::stable_sort(points.begin(), points.end(),
                     [](const std::pair<double, double>& a,
                        const std::pair<double, double>& b) {
                         return a.first < b.first;
                     });
    std::vector<std::pair<double, double>> unique;
    unique.reserve(points.size());
    for (const auto& point : points) {
        if (!unique.empty() && unique.back().first == point.first)
            unique.back() = point;  // duplicate frequency: last wins
        else
            unique.push_back(point);
    }

    // Fewer than two USABLE points carry no slope information → flat.
    // Non-positive frequencies are ignored as malformed (log2 undefined);
    // this keeps the pure function total for any input, never crashing.
    std::vector<double> logFreqs;
    logFreqs.reserve(unique.size());
    for (auto it = unique.begin(); it != unique.end();) {
        if (it->first <= 0.0)
            it = unique.erase(it);
        else {
            logFreqs.push_back(std::log2(it->first));
            ++it;
        }
    }
    if (unique.size() < 2)
        return result;

    const double logLow = logFreqs.front();
    const double logHigh = logFreqs.back();

    for (int band = 0; band < DeviceEqualizerBands::kBandCount; ++band) {
        const double logBand = std::log2(DeviceEqualizerBands::kFrequenciesHz[band]);
        // 0 dB outside the points' frequency span: a profile that says
        // nothing about a range does not touch it (edge folding of
        // out-of-range points happens via the in-span interpolation).
        if (logBand < logLow || logBand > logHigh)
            continue;
        // Piecewise-linear in log2(f): find the segment and interpolate.
        const auto upper = std::upper_bound(logFreqs.begin(), logFreqs.end(), logBand);
        const size_t j = static_cast<size_t>(upper - logFreqs.begin());
        // logLow <= logBand <= logHigh guarantees j >= 1 and j < size.
        const double t = (logBand - logFreqs[j - 1]) / (logFreqs[j] - logFreqs[j - 1]);
        double gain = unique[j - 1].second +
                      (unique[j].second - unique[j - 1].second) * t;
        // Defensive clamp into the profile gain domain (profiles from
        // make()/AutoEq are already valid; malformed input must not
        // exceed the domain either).
        gain = std::max(-EqualizerProfile::kMaxAbsGainDb,
                        std::min(EqualizerProfile::kMaxAbsGainDb, gain));
        result[band] = gain;
    }
    return result;
}

}  // namespace shanty
