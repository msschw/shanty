// SH-1008 (UX-19): see the header. Every command is a direct mirror of the
// shared SettingsFacade; no new library-config logic lives here (the
// existing tests/settings_vm cases stay the contract).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "ShantySettingsSingleton.h"

namespace shanty {

ShantySettingsSingleton::ShantySettingsSingleton(QObject* parent)
    : QObject(parent),
      libraryStore_(),
      settingsStore_(),
      effectChain_(),
      autoEqReader_(),
      facade_(settingsStore_, effectChain_, autoEqReader_, libraryStore_,
              this) {
    connect(&facade_, &SettingsFacade::libraryConfigChanged, this,
            &ShantySettingsSingleton::libraryConfigChanged);
    connect(&facade_, &SettingsFacade::rejected, this,
            &ShantySettingsSingleton::rejected);
}

void ShantySettingsSingleton::addFolder(const QString& path) {
    facade_.addFolder(path);
}

void ShantySettingsSingleton::removeFolder(const QString& path) {
    facade_.removeFolder(path);
}

void ShantySettingsSingleton::addWhitelistEntry(const QString& path) {
    facade_.addWhitelistEntry(path);
}

void ShantySettingsSingleton::removeWhitelistEntry(const QString& path) {
    facade_.removeWhitelistEntry(path);
}

void ShantySettingsSingleton::addBlacklistEntry(const QString& path) {
    facade_.addBlacklistEntry(path);
}

void ShantySettingsSingleton::removeBlacklistEntry(const QString& path) {
    facade_.removeBlacklistEntry(path);
}

void ShantySettingsSingleton::reloadLibraryConfig() {
    facade_.reloadLibraryConfig();
}

QString ShantySettingsSingleton::homeDir() const { return facade_.homeDir(); }

QVariantList ShantySettingsSingleton::listDirectory(
    const QString& path, const QVariantList& nameFilters) const {
    return facade_.listDirectory(path, nameFilters);
}

QVariantList ShantySettingsSingleton::libraryFolders() const {
    return facade_.libraryFolders();
}

QVariantList ShantySettingsSingleton::libraryWhitelist() const {
    return facade_.libraryWhitelist();
}

QVariantList ShantySettingsSingleton::libraryBlacklist() const {
    return facade_.libraryBlacklist();
}

}  // namespace shanty
