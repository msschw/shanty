// SH-605 Phase B: SettingsFacade — the QObject wrapper bridging the
// pure-C++ SettingsViewModel to QML. Thin: every view-model hook is
// mirrored to a Qt signal; rejections surface the view model's reason
// catalogue verbatim (the shell maps them to translated toasts).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// SH-D605-B1 (band editing + reload, decision recorded here): the view
// model's SH-605 contract has profile CRUD / selection / apply / reset /
// import but NO gain-editing entry point, while the settings page needs
// per-band sliders bound to the CURRENT profile. The facade therefore
// composes the edit from existing, tested pieces — the SH-501
// overwrite-by-name semantics of SettingsService::saveProfile (the same
// composite the view model itself uses for rename):
//   1. the facade's own SettingsService (over the SAME ISettingsStore)
//      persists EqualizerProfile::make(currentName, editedGains);
//   2. the view model is RE-CONSTRUCTED over the same store, which is the
//      SH-605 decision-6 reload path ("a view model reconstructed over the
//      same store re-reads the same profiles and selection") — the selection
//      under "equalizer/current" survives, and the cached profile list is
//      fresh.
// The facade OWNS its SettingsViewModel for exactly this reason (the other
// facades only hold references); the store, chain and reader are references
// owned by the object graph in app/shanty.cpp.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "core/ports/IAutoEqReader.h"
#include "core/ports/IEffectChain.h"
#include "core/ports/ISettingsStore.h"
#include "core/services/SettingsService.h"
#include "presentation/viewmodels/SettingsViewModel.h"

namespace shanty {

class SettingsFacade : public QObject {
    Q_OBJECT

    /// Persisted profiles (SH-501 load order) as QVariantMaps:
    ///   { name, gains: [double], bandCount, isCurrent,
    ///     source: "manual" | "imported", isValid }.
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    /// Empty string when nothing is selected.
    Q_PROPERTY(QString currentProfileName READ currentProfileName
                   NOTIFY profilesChanged)

public:
    explicit SettingsFacade(ISettingsStore& store, IEffectChain& chain,
                            IAutoEqReader& reader,
                            QObject* parent = nullptr);

    // ---- profile CRUD / selection / apply / reset (view-model contract) ---
    // Rejections surface through rejected(reason) with the view model's
    // catalogue strings; a successful apply/reset emits applied()/resetDone()
    // (the view model fires no hook on those success paths, so the facade
    // publishes them right after the call when no rejection occurred).
    Q_INVOKABLE void createProfile(const QString& name, int bandCount);
    Q_INVOKABLE void renameProfile(const QString& oldName,
                                   const QString& newName);
    Q_INVOKABLE void deleteProfile(const QString& name);
    Q_INVOKABLE void selectProfile(const QString& name);
    Q_INVOKABLE void applyCurrentProfile();
    Q_INVOKABLE void resetEqualizer();

    // ---- band editing (SH-D605-B1; see the class comment) -------------------
    /// Sets one band of the CURRENT profile (index into its gains list) and
    /// persists the profile under the same name. Reasons beyond the view
    /// model's catalogue: "band: no profile selected",
    /// "band: invalid band index", "band: rejected by store". The gain is
    /// clamped into the ±12 dB equalizer domain defensively (the UI slider
    /// already spans exactly that range).
    Q_INVOKABLE void setBandGain(int band, double gainDb);

    // ---- AutoEq import (view-model contract; the file -> bytes step is the
    //      application layer's job — the AutoEqReader adapter is Qt-free) ----
    /// Reads the file content and hands name + data + path verbatim to the
    /// view model. An unreadable file surfaces as a one-message
    /// importRejected (documented application-layer case); parse/persist
    /// outcomes mirror the view model hooks.
    Q_INVOKABLE void importAutoEqProfile(const QString& name,
                                         const QString& filePath);

    /// Home directory (FilePickerPage start point; same hook as on the
    /// Library/Playlist facades so the picker stays facade-agnostic).
    Q_INVOKABLE QString homeDir() const;
    /// Directory listing for FilePickerPage (see PickerSupport.h); the
    /// settings page passes this to its AutoEq import picker.
    Q_INVOKABLE QVariantList listDirectory(const QString& path,
                                           const QVariantList& nameFilters)
        const;

    // ---- Q_PROPERTY accessors ------------------------------------------------
    QVariantList profiles() const;
    QString currentProfileName() const;

signals:
    /// Fired after the stored profile list changed and was re-read, and
    /// after the current-profile selection actually changed (both NOTIFY
    /// properties derive from the same view-model state).
    void profilesChanged();
    /// Refused command (view-model onRejected mirror; reason catalogue
    /// strings verbatim, plus the facade's "band: …" reasons above).
    void rejected(const QString& reason);
    /// Successful AutoEq import (persisted name).
    void imported(const QString& name);
    /// Refused AutoEq import; nothing was persisted. One message per
    /// structured AutoEqError ("message (line N)").
    void importRejected(const QStringList& messages);
    /// applyCurrentProfile() completed without a rejection.
    void applied();
    /// resetEqualizer() completed (bypass + flat gains on the chain).
    void resetDone();

private:
    /// SH-D605-B1 reload path: re-constructs the view model over the same
    /// store (decision-6 reload), re-attaches the hooks and publishes.
    void rebuildViewModel();
    void connectViewModelCallbacks();
    const EqualizerProfile* findCurrentProfile() const;

    bool rejected_ = false;  // set by the onRejected mirror within one call
    ISettingsStore& store_;
    IEffectChain& chain_;
    IAutoEqReader& reader_;
    SettingsService profileService_;  // band-edit persistence (SH-D605-B1)
    std::unique_ptr<SettingsViewModel> viewModel_;
};

}  // namespace shanty
