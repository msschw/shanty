// SH-501: named equalizer profile storage on top of ISettingsStore.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "../domain/EqualizerProfile.h"
#include "../ports/ISettingsStore.h"

namespace shanty {

class SettingsService {
public:
    explicit SettingsService(ISettingsStore& store);

    /// Saves a profile; overwrites an existing profile with the same name.
    /// Invalid profiles are rejected (return false, no write).
    bool saveProfile(const EqualizerProfile& profile);

    /// Returns all stored profiles; empty when none exist.
    std::vector<EqualizerProfile> loadProfiles() const;

    bool deleteProfile(const std::string& name);

private:
    ISettingsStore& store_;
};

}  // namespace shanty
