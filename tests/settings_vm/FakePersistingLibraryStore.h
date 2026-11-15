// SH-1004 Phase A: persisting ILibraryStore fake for the settings view-model
// suite (house fake pattern from tests/fakes/, new file — shared fakes are
// not edited). Unlike tests/library_vm's recording FakeLibraryStore (which
// keeps the scripted load() result separate from save() for exact
// delegation assertions), this double behaves like a REAL store: load()
// returns the last saved config, so two view models over ONE instance see
// each other's writes through their reload paths — the cross-visibility
// contract SH-1004 locks. Save/load calls are still counted.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/domain/LibraryConfig.h"
#include "core/ports/ILibraryStore.h"

class FakePersistingLibraryStore : public shanty::ILibraryStore {
public:
    void save(const shanty::LibraryConfig& config) override {
        ++saveCount_;
        persisted_ = config;
    }

    shanty::LibraryConfig load() override {
        ++loadCount_;
        return persisted_;
    }

    /// The last saved config (what a fresh load() sees).
    shanty::LibraryConfig persisted_;
    int saveCount_ = 0;
    int loadCount_ = 0;
};
