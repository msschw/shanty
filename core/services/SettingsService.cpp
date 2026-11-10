// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsService.h"

namespace shanty {
// TODO(SH-501 Phase B): implement the persistence wire format (records the
// decision here once chosen — design D5). Tests only rely on the port.

SettingsService::SettingsService(ISettingsStore& store) : store_(store) {}

bool SettingsService::saveProfile(const EqualizerProfile&) {
    return false;  // unimplemented (tests must fail red)
}

std::vector<EqualizerProfile> SettingsService::loadProfiles() const {
    return std::vector<EqualizerProfile>();  // unimplemented (tests must fail red)
}

bool SettingsService::deleteProfile(const std::string&) {
    return false;  // unimplemented (tests must fail red)
}
}  // namespace shanty
