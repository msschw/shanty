// SH-204 adapter.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "core/ports/ILibraryStore.h"

namespace shanty {

// SH-204 Phase B: persists the LibraryConfig to a real settings file inside
// the app sandbox. See FileIndexStore.cpp for the file format and the
// location priority (SHANTY_LIBRARY_STORE env override, else the Qt
// GenericConfigLocation, which maps into the Sailfish app sandbox).
//
// Session semantics: a store instance that has not saved anything in this
// process represents a fresh library and returns an empty config, even when
// another instance inside the same process wrote the file. State written by
// a *different process* is adopted on load, so the configuration survives
// app restarts (the important cross-process case).
class FileIndexStore : public ILibraryStore {
public:
    // Optional explicit target file; defaults to the deterministic sandbox
    // location (SHANTY_LIBRARY_STORE env override, else the config path).
    explicit FileIndexStore(std::string filePath = std::string());

    void save(const LibraryConfig& config) override;
    LibraryConfig load() override;

private:
    std::string path_;              // resolved target file for this store
    bool savedInSession_ = false;   // this instance wrote path_ at least once
};

}  // namespace shanty
