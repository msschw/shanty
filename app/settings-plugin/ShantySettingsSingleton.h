// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// SH-1008 (UX-19): the page-facing singleton of the Shanty.Settings QML
// module. The OS Settings app loads the per-app page in ITS OWN QML engine
// (jolla-settings runs unsandboxed, in-process), so the app's context
// properties are not available there. This object exposes exactly the
// library-configuration surface that page needs — the same commands the
// in-app Settings page used to expose (SH-1004 UI, relocated by owner
// direction 2026-11-17) — and delegates to the app's SettingsFacade over
// the SAME FileIndexStore (single write path, same
// ~/.config/shanty/library.conf).
//
// Deliberately narrow: the equalizer/profile/AutoEq surface stays an
// app-only concern, so it is not registered for the Settings engine. The
// IEffectChain/IAutoEqReader the facade requires are inert stubs; the
// profile paths are unreachable from this surface.
#pragma once

#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QVariantList>

#include "adapters/settings/QSettingsSettingsStore.h"
#include "adapters/store/FileIndexStore.h"
#include "controllers/SettingsFacade.h"
#include "core/domain/AutoEqParseResult.h"
#include "core/ports/IAutoEqReader.h"
#include "core/ports/IEffectChain.h"

namespace shanty {

namespace detail {

/// Inert IEffectChain for the facade the Settings engine owns: the page
/// exposes no transport/equalizer commands, so applyGains/bypass are dead
/// code by construction (the view model only calls them on an explicit
/// apply/reset).
class NoOpEffectChain : public IEffectChain {
public:
    void applyGains(const std::vector<double>&) override {}
    void bypass() override {}
};

/// Inert IAutoEqReader (same reasoning: AutoEq import is app-only). A read
/// still returns a structured error rather than pretending to parse, so a
/// future surface cannot silently "succeed".
class UnavailableAutoEqReader : public IAutoEqReader {
public:
    AutoEqParseResult read(const std::string&, const std::string&) override {
        AutoEqParseResult result;
        result.errors.push_back(
            {AutoEqError::Code::Internal,
             std::string("AutoEq import is only available in the Shanty app"),
             0});
        return result;
    }
};

}  // namespace detail

class ShantySettingsSingleton : public QObject {
    Q_OBJECT

    /// The library-config lists (SH-1004 contract, same shapes as the
    /// in-app facade); libraryConfigChanged fires after each effective
    /// mutation.
    Q_PROPERTY(QVariantList libraryFolders READ libraryFolders
                   NOTIFY libraryConfigChanged)
    Q_PROPERTY(QVariantList libraryWhitelist READ libraryWhitelist
                   NOTIFY libraryConfigChanged)
    Q_PROPERTY(QVariantList libraryBlacklist READ libraryBlacklist
                   NOTIFY libraryConfigChanged)

public:
    explicit ShantySettingsSingleton(QObject* parent = nullptr);

    // ---- library configuration (thin mirrors of SettingsFacade) -----------
    Q_INVOKABLE void addFolder(const QString& path);
    Q_INVOKABLE void removeFolder(const QString& path);
    Q_INVOKABLE void addWhitelistEntry(const QString& path);
    Q_INVOKABLE void removeWhitelistEntry(const QString& path);
    Q_INVOKABLE void addBlacklistEntry(const QString& path);
    Q_INVOKABLE void removeBlacklistEntry(const QString& path);
    Q_INVOKABLE void reloadLibraryConfig();

    /// Home directory / directory listing for the shared FilePickerPage
    /// (same supports as the app's facades; see PickerSupport.h).
    Q_INVOKABLE QString homeDir() const;
    Q_INVOKABLE QVariantList listDirectory(const QString& path,
                                           const QVariantList& nameFilters)
        const;

    QVariantList libraryFolders() const;
    QVariantList libraryWhitelist() const;
    QVariantList libraryBlacklist() const;

signals:
    void libraryConfigChanged();
    /// Refused command (the view model's reason catalogue verbatim; the
    /// page surfaces it locally — no app-shell toast exists here).
    void rejected(const QString& reason);

private:
    // Declaration order matters: the facade references these.
    FileIndexStore libraryStore_;              // same file the app writes
    QSettingsSettingsStore settingsStore_;      // EQ store (facade requirement)
    detail::NoOpEffectChain effectChain_;
    detail::UnavailableAutoEqReader autoEqReader_;
    SettingsFacade facade_;
};

}  // namespace shanty
