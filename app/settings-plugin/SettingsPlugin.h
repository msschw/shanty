// SH-1008 (UX-19): QQmlExtensionPlugin entry point for the Shanty.Settings
// module. Loaded by the jolla-settings QML engine when the per-app page
// (Settings → Apps → Shanty) does `import Shanty.Settings 1.0`.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQmlExtensionPlugin>

namespace shanty {

class SettingsPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override;
};

}  // namespace shanty
