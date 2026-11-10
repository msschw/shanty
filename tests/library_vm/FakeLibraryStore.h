// SH-602 Phase A: recording ILibraryStore fake for the library view-model
// suite (house fake pattern from tests/fakes/, new file — tests/fakes/*.
// stays untouched). Records the load/save call counts and the last saved
// config so every delegation is asserted exactly.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/domain/LibraryConfig.h"
#include "core/ports/ILibraryStore.h"

class FakeLibraryStore : public shanty::ILibraryStore {
public:
    /// Starts with the scripted `loaded` config (see the suite fixture Vm).
    explicit FakeLibraryStore(shanty::LibraryConfig loaded = {})
        : loaded_(std::move(loaded)) {}

    void save(const shanty::LibraryConfig& config) override {
        ++saveCount_;
        lastSaved_ = config;
    }

    shanty::LibraryConfig load() override {
        ++loadCount_;
        return loaded_;
    }

    /// Scripted config returned by load().
    shanty::LibraryConfig loaded_;
    shanty::LibraryConfig lastSaved_;
    int loadCount_ = 0;
    int saveCount_ = 0;
};
