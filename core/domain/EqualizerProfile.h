// SH-501: equalizer profile domain type.
// Contract (tested): band gains live in [-12.0, +12.0] dB; flat() yields a
// "flat" profile with all gains at 0.0; isValid() reflects the band count
// and gain range validation result.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace shanty {

class EqualizerProfile {
public:
    static const int kDefaultBandCount = 10;
    static const double kMaxAbsGainDb;

    /// A profile where every band is 0.0 dB.
    static EqualizerProfile flat(const std::string& name = std::string(),
                                 int bandCount = kDefaultBandCount);

    /// Constructs a named profile from explicit band gains (dB).
    /// isValid() reports whether every gain is within
    /// [-kMaxAbsGainDb, +kMaxAbsGainDb].
    static EqualizerProfile make(const std::string& name,
                                 const std::vector<double>& gainsDb);

    const std::string& name() const { return name_; }
    const std::vector<double>& gains() const { return gains_; }
    // SH-501 Phase B: the inline stub body was hardcoded to `false`; the
    // signature is unchanged, only the (fixed-header) body now delegates to
    // the validity flag set by the factories implemented in the .cpp.
    bool isValid() const { return valid_; }

private:
    std::string name_;
    std::vector<double> gains_;
    /// Cached validation result, set by flat()/make(). Default-constructed
    /// (and zero-band) profiles are invalid.
    bool valid_ = false;
};

}  // namespace shanty
