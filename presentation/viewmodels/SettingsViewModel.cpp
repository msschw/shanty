// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later

// SH-605 Phase B: SettingsViewModel implementation, following the decision
// record in SettingsViewModel.h (each decision's assertion lives in
// tests/settings_vm/tst_settings_vm.cpp). Callbacks instead of exceptions;
// every refusal is a complete no-op on the store and on the chain.

#include "SettingsViewModel.h"

#include "../../core/ports/IAutoEqReader.h"
#include "../../core/ports/IEffectChain.h"
#include "../../core/ports/ISettingsStore.h"
#include "../../core/services/EffectChainService.h"
#include "../../core/services/SettingsService.h"

namespace shanty {

namespace {

/// Decision 2: the selection is a view-model-owned setting in the
/// "equalizer/" namespace (see SettingsService.cpp's key-namespace note,
/// which anticipated "player options in SH-605").
const char kCurrentKey[] = "equalizer/current";

}  // namespace

const char* SettingsViewModel::currentKey() { return kCurrentKey; }

SettingsViewModel::SettingsViewModel(ISettingsStore& store, IEffectChain& chain,
                                     IAutoEqReader& reader)
    : store_(store),
      chain_(chain),
      reader_(reader),
      profileService_(store) {
    // Decision 6: construction IS the reload — profiles come from the
    // service, the selection from the view model's own key.
    profiles_ = profileService_.loadProfiles();
    const std::map<std::string, std::string> persisted = store_.readAll();
    const auto it = persisted.find(kCurrentKey);
    if (it != persisted.end()) {
        // A stale persisted name (profile deleted by another process) is
        // dropped in memory only; the key itself is left for the next save
        // to fix (decision 2).
        if (findProfile(it->second) != nullptr) {
            currentName_ = it->second;
        }
    }
}

const std::vector<EqualizerProfile>& SettingsViewModel::profiles() const {
    return profiles_;
}

void SettingsViewModel::createProfile(const std::string& name, int bandCount) {
    if (name.empty()) {
        rejected("create: empty profile name");
        return;
    }
    if (bandCount <= 0) {
        rejected("create: invalid band count");
        return;
    }
    if (findProfile(name) != nullptr) {
        rejected("create: profile name already exists");
        return;
    }

    // Decision 1: a flat profile is a valid, immediately editable start;
    // bandCount > 0 guarantees validity, so a save rejection cannot happen
    // (guard kept defensive — saveProfile never wrote in that case).
    if (!profileService_.saveProfile(EqualizerProfile::flat(name, bandCount))) {
        rejected("create: rejected by store");
        return;
    }
    profiles_ = profileService_.loadProfiles();
    if (onProfilesChanged) {
        onProfilesChanged();
    }
    // Decision 2: a fresh profile is created to be tuned → auto-selected.
    selectProfile(name);
}

void SettingsViewModel::renameProfile(const std::string& oldName,
                                      const std::string& newName) {
    const EqualizerProfile* old = findProfile(oldName);
    if (old == nullptr) {
        rejected("rename: unknown profile");
        return;
    }
    if (newName.empty()) {
        rejected("rename: empty profile name");
        return;
    }
    if (newName == oldName) {
        return;  // redundant rename: nothing to move, no store traffic
    }
    if (findProfile(newName) != nullptr) {
        rejected("rename: name already exists");
        return;
    }

    // Decision 1: composite of the SH-501 overwrite-by-name semantics;
    // gains and ProfileSource are preserved, only the name moves.
    const EqualizerProfile moved = EqualizerProfile::makeWithSource(
        newName, old->gains(), old->source());
    if (!profileService_.saveProfile(moved)) {
        rejected("rename: rejected by store");
        return;
    }
    // The old record existed at load, so the delete cannot fail here.
    profileService_.deleteProfile(oldName);
    profiles_ = profileService_.loadProfiles();
    if (onProfilesChanged) {
        onProfilesChanged();
    }
    if (currentName_ == oldName) {
        currentName_ = newName;
        store_.write(kCurrentKey, newName);
        if (onSelectionChanged) {
            onSelectionChanged();
        }
    }
}

void SettingsViewModel::deleteProfile(const std::string& name) {
    if (!profileService_.deleteProfile(name)) {
        rejected("delete: unknown profile");
        return;
    }
    profiles_ = profileService_.loadProfiles();
    if (onProfilesChanged) {
        onProfilesChanged();
    }
    if (currentName_ == name) {
        // Decision 2: no fallback — a silent swap to another profile would
        // misapply the user's chain.
        currentName_.clear();
        store_.remove(kCurrentKey);
        if (onSelectionChanged) {
            onSelectionChanged();
        }
    }
}

void SettingsViewModel::selectProfile(const std::string& name) {
    if (findProfile(name) == nullptr) {
        rejected("select: unknown profile");
        return;
    }
    if (name == currentName_) {
        return;  // redundant re-select: no write, no hook (SH-404 pattern)
    }
    currentName_ = name;
    store_.write(kCurrentKey, name);
    if (onSelectionChanged) {
        onSelectionChanged();
    }
}

const std::string& SettingsViewModel::currentProfileName() const {
    return currentName_;
}

void SettingsViewModel::applyCurrentProfile() {
    if (currentName_.empty()) {
        rejected("apply: no profile selected");
        return;
    }
    const EqualizerProfile* profile = findProfile(currentName_);
    if (profile == nullptr) {
        rejected("apply: unknown profile");
        return;
    }
    if (!profile->isValid()) {
        rejected("apply: invalid profile");
        return;
    }

    // Decision 3: the view model owns the band-count construance — the
    // service is constructed per apply with the chain sized to the current
    // profile's band count (gains().size(); EqualizerProfile has no
    // bandCount() accessor). No transport is driven: the view model owns
    // only the EQ section.
    EffectChainService effectChain(chain_,
                                   static_cast<int>(profile->gains().size()));
    if (!effectChain.applyProfile(*profile)) {
        // Unreachable with the profile-derived sizing above; kept as the
        // documented surfacing path for a future fixed-grid wiring.
        rejected("apply: rejected by effect chain");
        return;
    }
}

void SettingsViewModel::resetEqualizer() {
    // Decision 7: flat reset sized to the selected profile's band count so
    // it matches the last applied mapping; default band count otherwise.
    int bandCount = EqualizerProfile::kDefaultBandCount;
    const EqualizerProfile* profile = findProfile(currentName_);
    if (profile != nullptr && profile->isValid()) {
        bandCount = static_cast<int>(profile->gains().size());
    }
    EffectChainService(chain_, bandCount).reset();
}

void SettingsViewModel::importAutoEqProfile(const std::string& name,
                                            const std::string& data,
                                            const std::string& filePath) {
    if (name.empty()) {
        rejected("import: empty profile name");
        return;
    }

    const AutoEqParseResult result = reader_.read(data, filePath);
    if (!result.ok()) {
        // Decision 5: reader errors are forwarded verbatim as structured
        // values; nothing is persisted.
        if (onImportRejected) {
            onImportRejected(result.errors);
        }
        return;
    }

    const EqualizerProfile imported = EqualizerProfile::makeWithSource(
        name, result.profile.gains(), result.profile.source());
    if (!profileService_.saveProfile(imported)) {
        // Parse-ok but non-persistable (gains outside the ±12 dB domain).
        // saveProfile refused BEFORE touching the store (SH-501), so the
        // failure is purely a surfaced value; Code::Unknown is the closest
        // SH-503 code — the message states the actual cause.
        std::vector<AutoEqError> errors(1);
        errors[0].code = AutoEqError::Code::Unknown;
        errors[0].message =
            "parsed profile is not persistable: gains outside the "
            "-12..+12 dB equalizer range";
        errors[0].line = 0;
        if (onImportRejected) {
            onImportRejected(errors);
        }
        return;
    }

    profiles_ = profileService_.loadProfiles();
    if (onProfilesChanged) {
        onProfilesChanged();
    }
    if (onImported) {
        onImported(name);
    }
}

void SettingsViewModel::rejected(const char* reason) {
    if (onRejected) {
        onRejected(reason);
    }
}

const EqualizerProfile* SettingsViewModel::findProfile(
    const std::string& name) const {
    for (const EqualizerProfile& profile : profiles_) {
        if (profile.name() == name) {
            return &profile;
        }
    }
    return nullptr;
}

}  // namespace shanty
