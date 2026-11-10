// SH-203 adapter.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/ILibraryScanner.h"

namespace shanty {

class FilesystemScanner : public ILibraryScanner {
public:
    std::vector<std::string> scan(
        const std::vector<std::string>& folders) override;
};

}  // namespace shanty
