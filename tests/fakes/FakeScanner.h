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

    // SH-1002 Phase A stub: separate playlist enumeration query. The
    // playlist result and its own call counter stay controllable from
    // the tests; Phase B keeps this shape.
    std::vector<std::string> scanPlaylists(
        const std::vector<std::string>&) override {
        ++playlistCallCount_;
        return playlistResult_;
    }

    std::vector<std::string> result_;
    int callCount_ = 0;

    std::vector<std::string> playlistResult_;
    int playlistCallCount_ = 0;
};
