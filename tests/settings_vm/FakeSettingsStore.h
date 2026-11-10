// SH-605 test double: a real in-memory ISettingsStore (the SH-501
// InMemorySettingsStore house pattern, local copy per the ownership rules —
// shared fakes are not edited) that additionally RECORDS every call so the
// SettingsViewModel delegations can be asserted by call count and by exact
// key/value arguments.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/ports/ISettingsStore.h"

class FakeSettingsStore : public shanty::ISettingsStore {
public:
    std::map<std::string, std::string> data;

    // ---- recording ---------------------------------------------------------
    std::vector<std::pair<std::string, std::string>> writeCalls;
    std::vector<std::string> removeCalls;
    int readAllCount = 0;

    // ---- ISettingsStore ----------------------------------------------------
    std::map<std::string, std::string> readAll() override {
        ++readAllCount;
        return data;
    }

    void write(const std::string& key, const std::string& value) override {
        writeCalls.emplace_back(key, value);
        data[key] = value;
    }

    void remove(const std::string& key) override {
        removeCalls.push_back(key);
        data.erase(key);
    }
};
