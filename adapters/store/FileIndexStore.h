// SH-204 adapter.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/ILibraryStore.h"

namespace shanty {

class FileIndexStore : public ILibraryStore {
public:
    void save(const LibraryConfig& config) override;
    LibraryConfig load() override;

private:
    LibraryConfig lastSaved_;
};

}  // namespace shanty
