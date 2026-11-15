// SH-1003 Phase A: scripted ITagReader fake for the catalogue suite (the
// SH-1001 port double). Paths listed in the map return their scripted
// TagMetadata; any other path yields the documented CLEAN MISS (an empty
// TagMetadata) — exactly the adapter contract. Every call is counted so
// the "tags re-read exactly once per track per rebuild" decision is
// asserted exactly. New file (house fake pattern) — tests/fakes/* stays
// untouched.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>
#include <utility>

#include "core/domain/TagMetadata.h"
#include "core/ports/ITagReader.h"

class FakeTagReader : public shanty::ITagReader {
public:
    explicit FakeTagReader(
        std::map<std::string, shanty::TagMetadata> scripted = {})
        : scripted_(std::move(scripted)) {}

    shanty::TagMetadata read(const std::string& path) override {
        ++callCount_;
        ++readsPerPath_[path];
        const std::map<std::string, shanty::TagMetadata>::const_iterator it =
            scripted_.find(path);
        if (it == scripted_.end()) {
            return shanty::TagMetadata();  // clean miss
        }
        return it->second;
    }

    /// How many times each path was read (decision 1: exactly one per
    /// track per rebuild).
    int readsFor(const std::string& path) const {
        const std::map<std::string, int>::const_iterator it =
            readsPerPath_.find(path);
        return it == readsPerPath_.end() ? 0 : it->second;
    }

    std::map<std::string, shanty::TagMetadata> scripted_;
    int callCount_ = 0;

private:
    std::map<std::string, int> readsPerPath_;
};
