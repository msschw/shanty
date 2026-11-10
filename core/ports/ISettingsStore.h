// SH-501 port: generic key/value settings persistence (design D5's store).
// Implementations must provide atomic updates and survive process restarts.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>

namespace shanty {

class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;

    virtual std::map<std::string, std::string> readAll() = 0;
    virtual void write(const std::string& key, const std::string& value) = 0;
    virtual void remove(const std::string& key) = 0;
};

}  // namespace shanty
