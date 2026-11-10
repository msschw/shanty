// SH-605: settings view-model — the logic layer behind the Silica settings
// page (player options / EQ profiles / AutoEq import). Pure C++ with
// callbacks, exactly like core/playback/PlaybackController (SH-404): no Qt,
// no exceptions; the QML/Silica wiring is Phase B-of-the-epic work by the
// epic owner and must hold a reference to this class only.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Decisions record (each exercised by tests/settings_vm/tst_settings_vm.cpp):
//
//  1. PROFILE CRUD delegates to SettingsService (SH-501):
//       create  → EqualizerProfile::flat(name, bandCount) persisted via
//                 saveProfile() (flat is a valid, immediately editable start);
//       rename  → saveProfile(renamed) + deleteProfile(old): the service has
//                 no atomic rename, and per the ownership rules this ticket
//                 composites the SH-501 overwrite-by-name semantics instead
//                 of extending the service (gains and ProfileSource are
//                 preserved; only the name moves);
//       delete  → deleteProfile(), whose own "unknown profile" result is
//                 surfaced through onRejected().
//  2. CURRENT-PROFILE SELECTION is a view-model-owned setting. SH-501's
//     service exposes profile CRUD only, so the selection is persisted by
//     the view model itself under the store key "equalizer/current" (the
//     "equalizer/" key namespace precedent was documented in
//     SettingsService.cpp, which anticipated "player options in SH-605").
//     Which profile is current:
//       - createProfile() AUTO-SELECTS the new profile (a fresh profile is
//         created to be tuned/applied);
//       - importing does NOT auto-select (an import extends the library);
//       - deleting the selected profile CLEARS the selection with no
//         fallback (silently applying another profile would misapply the
//         user's chain);
//       - re-selecting the already-current name is a redundant no-op (no
//         store write, no hook) — PlaybackController's redundancy rule;
//       - at load, a persisted selection that no longer matches a stored
//         profile is dropped in memory only (a stale name is never
//         re-written; the stale key is left for the next save to fix).
//  3. BAND-COUNT CONSTRUANCE (the glue decision this ticket owns): the
//     IEffectChain port has no band-count query (SH-502 decision 1) and the
//     view-model layer cannot know the device chain's fixed layout, so
//     applyCurrentProfile() constructs EffectChainService with the chain
//     sized to the CURRENT profile's band count (EqualizerProfile has no
//     bandCount() accessor; gains().size() IS the band count — every
//     profile factory keeps the two in sync). With that construance a
//     band-count mismatch cannot occur by construction; converting
//     imported wide AutoEq profiles onto a fixed device grid remains
//     out-of-scope (SH-503 record: no resampler). applyProfile() can still
//     be rejected (no selection, unknown/invalid current profile, or a
//     defensive false from the service), and ANY rejection is surfaced via
//     onRejected() while the chain is left untouched.
//  4. onRejected(const char* reason) reason catalogue (stable strings for
//     the Phase B QML layer, PlaybackController style). Every refusal is a
//     COMPLETE no-op: no store write, no chain call, no cached-state change.
//       "create: empty profile name"      "create: invalid band count"
//       "create: profile name already exists"  "create: rejected by store"
//       "rename: unknown profile"         "rename: empty profile name"
//       "rename: name already exists"     "rename: rejected by store"
//       "delete: unknown profile"
//       "select: unknown profile"
//       "apply: no profile selected"      "apply: unknown profile"
//       "apply: invalid profile"          "apply: rejected by effect chain"
//       "import: empty profile name"
//  5. AUTOEQ IMPORT: importAutoEqProfile() hands the file content and path
//     VERBATIM to the injected IAutoEqReader (format expertise lives in
//     SH-503), stamps the CALLER-supplied display name onto the parsed
//     gains while KEEPING the reader's source tracking
//     (ProfileSource::Imported by the reader's contract), and persists the
//     profile through SettingsService. Importing over an existing name
//     overwrites it (SH-501 save semantics — re-importing an updated
//     AutoEq export updates the stored profile). Failures surface as
//     structured VALUES, never exceptions:
//       - reader errors → the AutoEqParseResult error list is forwarded to
//         onImportRejected() verbatim; nothing is persisted;
//       - a parse-ok profile whose gains fall outside the ±12 dB equalizer
//         domain cannot be persisted (saveProfile refuses invalid profiles,
//         SH-501) → onImportRejected() receives ONE synthesized
//         AutoEqError (Code::Unknown — the SH-503 enum has no dedicated
//         out-of-range code; the message states the cause). Same contract:
//         no store write happened (SH-501 "reject before touching the
//         store").
//  6. PERSISTENCE / RELOAD CONTRACT: only the profiles
//     ("equalizer/profiles", through SettingsService) and the selection
//     ("equalizer/current") persist. The chain's current gains are
//     RUNTIME-ONLY backend state and are never persisted; a view model
//     reconstructed over the same store re-reads the same profiles and
//     selection ("changes survive reload", asserted round-trip through the
//     fake store) and reproduces the chain state by a fresh apply.
//  7. resetEqualizer() is the "bypass / apply-flat" action: it constructs
//     EffectChainService with the band count of the validly selected
//     current profile (so the flat gains match the last applied mapping),
//     else with EqualizerProfile::kDefaultBandCount; both SH-502 bypass
//     mechanisms fire (chain.bypass() first, then explicit all-zero gains).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../../core/domain/AutoEqParseResult.h"
#include "../../core/domain/EqualizerProfile.h"
#include "../../core/services/SettingsService.h"

namespace shanty {

class IAutoEqReader;
class IEffectChain;
class ISettingsStore;


class SettingsViewModel {
public:
    /// @param store the persisted settings backend (shared with
    ///     SettingsService, which the view model constructs over it);
    /// @param chain the EQ effect chain the view model drives (decisions 3
    ///     and 7); the view model never drives any transport;
    /// @param reader the AutoEq profile reader (decision 5).
    SettingsViewModel(ISettingsStore& store, IEffectChain& chain,
                      IAutoEqReader& reader);

    /// Store key of the persisted current-profile selection (decision 2).
    static const char* currentKey();

    // ---- observation hooks (no exceptions for flow control) ----------------
    /// Fired after the stored profile list changed and was re-read.
    std::function<void()> onProfilesChanged;
    /// Fired after the current-profile selection actually changed.
    std::function<void()> onSelectionChanged;
    /// Fired for a command that was refused as a documented no-op
    /// (reason catalogue: decision 4).
    std::function<void(const char* reason)> onRejected;
    /// Fired with the persisted name after a successful AutoEq import.
    std::function<void(const std::string& name)> onImported;
    /// Fired with the structured errors after a refused AutoEq import
    /// (decision 5); nothing was persisted.
    std::function<void(const std::vector<AutoEqError>& errors)>
        onImportRejected;

    // ---- profile list (persisted through SettingsService) ------------------
    /// All persisted profiles in SH-501 load order.
    const std::vector<EqualizerProfile>& profiles() const;

    /// Creates a flat profile (decision 1) and auto-selects it (decision 2).
    void createProfile(const std::string& name,
                       int bandCount = EqualizerProfile::kDefaultBandCount);
    /// Moves a profile to a new name, preserving gains and source
    /// (decision 1); keeps the selection if the renamed profile was current.
    void renameProfile(const std::string& oldName, const std::string& newName);
    /// Deletes a profile (decision 1); clears the selection when the deleted
    /// profile was current (decision 2).
    void deleteProfile(const std::string& name);

    // ---- current-profile selection (decision 2) ----------------------------
    void selectProfile(const std::string& name);
    /// Empty string when nothing is selected.
    const std::string& currentProfileName() const;

    // ---- effect-chain application (decisions 3 and 7) ----------------------
    /// Applies the current profile's gains to the chain in band order,
    /// through an EffectChainService sized to the profile's band count.
    void applyCurrentProfile();
    /// Bypass + flat request (decision 7).
    void resetEqualizer();

    // ---- AutoEq import (decision 5) ----------------------------------------
    void importAutoEqProfile(const std::string& name, const std::string& data,
                             const std::string& filePath = std::string());

private:
    void rejected(const char* reason);
    const EqualizerProfile* findProfile(const std::string& name) const;

    ISettingsStore& store_;
    IEffectChain& chain_;
    IAutoEqReader& reader_;
    SettingsService profileService_;
    std::vector<EqualizerProfile> profiles_;
    std::string currentName_;
};

}  // namespace shanty
