// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "EqualizerProfile.h"

namespace shanty {
// SH-501 Phase B: validation and construction rules.
//
// Contract (tested once more here in comments; tests live in
// tests/equalizer_profile):
//   - kMaxAbsGainDb = 12.0; every band gain must lie in [-12, +12] dB.
//   - flat(name, bandCount) yields all-zero gains; bandCount <= 0 is a
//     dimension error and produces an invalid profile.
//   - make(name, gainsDb) is invalid when any |gain| exceeds kMaxAbsGainDb
//     or the gains vector is empty.
// The validation result is cached in `valid_` at construction time; profiles
// are immutable afterwards.

const double EqualizerProfile::kMaxAbsGainDb = 12.0;

namespace {

/// Rejects out-of-range gains; a NaN fails both comparisons, so it is also
/// rejected (NaN is never a meaningful equalizer gain).
bool gainsInRange(const std::vector<double>& gains) {
    for (double gain : gains) {
        if (!(gain >= -EqualizerProfile::kMaxAbsGainDb) ||
            !(gain <= EqualizerProfile::kMaxAbsGainDb)) {
            return false;
        }
    }
    return true;
}

}  // namespace

EqualizerProfile EqualizerProfile::flat(
    const std::string& name /* = std::string() */,
    int bandCount /* = kDefaultBandCount */) {
    EqualizerProfile profile;
    profile.name_ = name;
    if (bandCount > 0) {
        profile.gains_.assign(static_cast<std::size_t>(bandCount), 0.0);
        profile.valid_ = true;
    }
    // bandCount <= 0 stays an empty, invalid profile (dimension error).
    return profile;
}

EqualizerProfile EqualizerProfile::make(
    const std::string& name,
    const std::vector<double>& gainsDb) {
    EqualizerProfile profile;
    profile.name_ = name;
    profile.gains_ = gainsDb;
    profile.valid_ = !gainsDb.empty() && gainsInRange(gainsDb);
    return profile;
}

// SH-503 Phase B (additive): origin-tracking construction; validation
// rules are identical to make().
EqualizerProfile EqualizerProfile::makeWithSource(
    const std::string& name,
    const std::vector<double>& gainsDb,
    ProfileSource source) {
    EqualizerProfile profile = make(name, gainsDb);
    profile.source_ = source;
    return profile;
}

}  // namespace shanty
