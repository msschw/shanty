// SH-E6 Phase B (SH-605 application wiring): QSettings-backed ISettingsStore
// adapter — the concrete device-buildable settings persistence. SH-501 owns
// the wire format (a single "equalizer/profiles" value + the view-model's
// "equalizer/current" key); this adapter only maps the port's string->string
// map onto QSettings. QtCore-only, host-compilable.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <string>

#include "core/ports/ISettingsStore.h"

class QSettings;

namespace shanty {

/// ISettingsStore over QSettings (INI format). Keys may contain '/' —
/// QSettings maps it onto nested groups, and readAll() reports the full
/// flattened key paths ("equalizer/profiles"), so the port's map semantics
/// are preserved end-to-end. Values are strings verbatim (newlines/tabs in
/// the SH-501 profile wire format are escaped by QSettings' own INI
/// serialisation and round trip exactly).
///
/// Storage location mirrors FileIndexStore (SH-204):
///   1. The SHANTY_SETTINGS_STORE env var overrides the absolute file path
///      (documented hook for development/tests; unset on the device).
///   2. Otherwise <GenericConfigLocation>/shanty/settings.ini — inside the
///      app sandbox on Sailfish, ~/.config/shanty/settings.ini on a host.
///      No absolute device paths are hardcoded.
class QSettingsSettingsStore : public ISettingsStore {
public:
    /// @param filePath absolute INI file path; empty selects the default
    ///     location described above.
    explicit QSettingsSettingsStore(std::string filePath = std::string());
    ~QSettingsSettingsStore() override;

    QSettingsSettingsStore(const QSettingsSettingsStore&) = delete;
    QSettingsSettingsStore& operator=(const QSettingsSettingsStore&) = delete;

    // ---- ISettingsStore ----------------------------------------------------
    std::map<std::string, std::string> readAll() override;
    void write(const std::string& key, const std::string& value) override;
    void remove(const std::string& key) override;

private:
    std::unique_ptr<QSettings> settings_;
};

}  // namespace shanty
