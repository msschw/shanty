// SH-602 Phase A: recording ILibraryScanner fake for the library view-model
// suite (FakeScanner pattern from tests/fakes/, extended to record the
// `folders` argument of every call so the view-model delegation is asserted
// exactly). New file — tests/fakes/* stays untouched.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/ports/ILibraryScanner.h"

class RecordingScanner : public shanty::ILibraryScanner {
public:
    explicit RecordingScanner(std::vector<std::string> result)
        : result_(std::move(result)) {}

    std::vector<std::string> scan(
        const std::vector<std::string>& folders) override {
        ++callCount_;
        scannedFolders_.push_back(folders);
        return result_;
    }

    // SH-1002 Phase A compile-fix stub: the port gained the separate
    // playlist query; this fake keeps returning nothing (red-first
    // suites assert against real implementations elsewhere).
    std::vector<std::string> scanPlaylists(
        const std::vector<std::string>& folders) override {
        ++playlistCallCount_;
        scannedPlaylistFolders_.push_back(folders);
        return {};
    }

    std::vector<std::string> result_;
    int callCount_ = 0;
    std::vector<std::vector<std::string>> scannedFolders_;

    int playlistCallCount_ = 0;
    std::vector<std::vector<std::string>> scannedPlaylistFolders_;
};
