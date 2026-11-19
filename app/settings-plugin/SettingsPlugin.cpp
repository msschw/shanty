// SH-1008 (UX-19): module registration. The singleton is created lazily by
// the engine (one instance per engine) and parented to it; it owns the
// library store and the SettingsFacade it exposes.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsPlugin.h"

#include <QtQml>

#include "ShantySettingsSingleton.h"

namespace shanty {

void SettingsPlugin::registerTypes(const char *uri) {
    // The qmldir declares exactly this module; guard the contract so a
    // copy/paste rename cannot register the singleton under another URI.
    Q_ASSERT(QString::fromLatin1(uri) == QStringLiteral("Shanty.Settings"));

    qmlRegisterSingletonType<ShantySettingsSingleton>(
        uri, 1, 0, "ShantySettings",
        [](QQmlEngine *engine, QJSEngine *) -> QObject * {
            return new ShantySettingsSingleton(engine);
        });
}

}  // namespace shanty
