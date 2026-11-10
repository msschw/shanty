// Shared test double for SH-501. Copyright 2026 Shanty contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>

#include "core/ports/ISettingsStore.h"

/// A real (non-stub-logic) in-memory store: the fake is trusted, the service
/// under test is what Phase A exercises.
class InMemorySettingsStore : public shanty::ISettingsStore {
public:
    std::map<std::string, std::string> data;

    std::map<std::string, std::string> readAll() override { return data; }

    void write(const std::string& key, const std::string& value) override {
        data[key] = value;
        ++writeCount;
    }

    void remove(const std::string& key) override {
        data.erase(key);
        ++removeCount;
    }

    int writeCount = 0;
    int removeCount = 0;
};
