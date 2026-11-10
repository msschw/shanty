// Shared test double for SH-202. Copyright 2026 Shanty contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/ILibraryScanner.h"

class FakeScanner : public shanty::ILibraryScanner {
public:
    explicit FakeScanner(std::vector<std::string> result)
        : result_(std::move(result)) {}

    std::vector<std::string> scan(const std::vector<std::string>&) override {
        ++callCount_;
        return result_;
    }

    std::vector<std::string> result_;
    int callCount_ = 0;
};
