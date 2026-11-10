// SH-501: equalizer profile domain type.
// Contract (tested): band gains live in [-12.0, +12.0] dB; flat() yields a
// "flat" profile with all gains at 0.0; isValid() reflects the band count
// and gain range validation result.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace shanty {

// SH-503 (additive): origin tracking of an equalizer profile — tuned by
// hand in the app vs. imported from an AutoEq profile file (§1bis:
// "profile source tracked (imported vs. manual)"). Purely additive to the
// SH-501 contract: default stays Manual for every existing factory, and
// all existing signatures are unchanged.
enum class ProfileSource { Manual, Imported };

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

    /// SH-503 (additive): same construction/validation rules as make(),
    /// plus explicit origin metadata (see ProfileSource). Existing
    /// signatures stay untouched; make() keeps its Manual default.
    static EqualizerProfile makeWithSource(
        const std::string& name,
        const std::vector<double>& gainsDb,
        ProfileSource source);

    const std::string& name() const { return name_; }
    const std::vector<double>& gains() const { return gains_; }
    // SH-501 Phase B: the inline stub body was hardcoded to `false`; the
    // signature is unchanged, only the (fixed-header) body now delegates to
    // the validity flag set by the factories implemented in the .cpp.
    bool isValid() const { return valid_; }

    // SH-503 (additive accessor): where this profile came from (SH-503
    // Phase B wires the importer through to set Imported; see
    // EqualizerProfile::makeWithSource there). Default is Manual — the
    // tests/equalizer_profile suite keeps asserting the SH-501 behaviour.
    ProfileSource source() const { return source_; }

private:
    std::string name_;
    std::vector<double> gains_;
    // SH-503 (additive field): origin metadata. Default Manual; the SH-503
    // Phase B importer stamps Imported at construction (via the additive
    // makeWithSource factory, see EqualizerProfile.cpp).
    ProfileSource source_ = ProfileSource::Manual;
    /// Cached validation result, set by flat()/make(). Default-constructed
    /// (and zero-band) profiles are invalid.
    bool valid_ = false;
};

}  // namespace shanty
