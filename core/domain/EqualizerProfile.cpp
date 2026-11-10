// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "EqualizerProfile.h"

namespace shanty {
// TODO(SH-501 Phase B): implement validation and construction rules.

const double EqualizerProfile::kMaxAbsGainDb = 12.0;

EqualizerProfile EqualizerProfile::flat(const std::string&, int) {
    return EqualizerProfile();  // unimplemented (tests must fail red)
}

EqualizerProfile EqualizerProfile::make(const std::string&,
                                        const std::vector<double>&) {
    return EqualizerProfile();  // unimplemented (tests must fail red)
}
}  // namespace shanty
