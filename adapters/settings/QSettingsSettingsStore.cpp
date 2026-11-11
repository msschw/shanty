// SH-E6 Phase B (SH-605 application wiring): QSettingsSettingsStore
// implementation — see the location/format notes in the header.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "QSettingsSettingsStore.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

#include <map>
#include <utility>

namespace shanty {

namespace {

std::string defaultStorePath() {
    // Qt 5.6 compat (device target): qEnvironmentVariable() is Qt >= 5.10;
    // the qEnvironmentVariableIsSet()/qgetenv() pair is the documented
    // replacement (same pattern as FileIndexStore.cpp, SH-204).
    const QString overridePath =
        qEnvironmentVariableIsSet("SHANTY_SETTINGS_STORE")
            ? QString::fromLocal8Bit(qgetenv("SHANTY_SETTINGS_STORE"))
            : QString();
    if (!overridePath.isEmpty()) {
        return overridePath.toStdString();
    }
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    if (base.isEmpty()) {
        // No config location available (exotic sandbox setups): fall back to
        // the per-user temp dir rather than polluting the working directory.
        base = QDir::tempPath();
    }
    return (base + "/shanty/settings.ini").toStdString();
}

}  // namespace

QSettingsSettingsStore::QSettingsSettingsStore(std::string filePath)
    : settings_(new QSettings(
          QString::fromStdString(filePath.empty() ? defaultStorePath()
                                                  : std::move(filePath)),
          QSettings::IniFormat)) {}

QSettingsSettingsStore::~QSettingsSettingsStore() = default;

std::map<std::string, std::string> QSettingsSettingsStore::readAll() {
    std::map<std::string, std::string> result;
    // allKeys() reports every key across all groups with its full
    // '/'-separated path ("equalizer/profiles"), matching the port's flat
    // key space.
    const QStringList keys = settings_->allKeys();
    for (const QString& key : keys) {
        result[key.toStdString()] =
            settings_->value(key).toString().toStdString();
    }
    return result;
}

void QSettingsSettingsStore::write(const std::string& key,
                                   const std::string& value) {
    settings_->setValue(QString::fromStdString(key),
                        QString::fromStdString(value));
    settings_->sync();  // persist promptly (crash safety, port contract)
}

void QSettingsSettingsStore::remove(const std::string& key) {
    settings_->remove(QString::fromStdString(key));
    settings_->sync();
}

}  // namespace shanty
