// SH-1003 Phase A: persisting ILibraryStore fake for the catalogue suite
// (same double as tests/settings_vm's — load() returns the last saved
// config, load/save counted; new file, house pattern — tests/fakes/*
// stays untouched).
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

    shanty::LibraryConfig persisted_;
    int saveCount_ = 0;
    int loadCount_ = 0;
};
