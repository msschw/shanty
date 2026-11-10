// SH-204 port: persistence of the LibraryConfig.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../domain/LibraryConfig.h"

namespace shanty {

class ILibraryStore {
public:
    virtual ~ILibraryStore() = default;

    virtual void save(const LibraryConfig& config) = 0;
    virtual LibraryConfig load() = 0;
};

}  // namespace shanty
