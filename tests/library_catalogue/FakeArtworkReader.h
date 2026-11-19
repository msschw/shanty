// SH-1009 Phase A (UX-20): scripted IArtworkReader fake for the browser
// artwork-resolution cases. Paths listed in the map yield a non-empty
// Artwork; any other path is the documented clean miss. Reads are counted
// per path so the per-rebuild presence cache can be asserted.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>

#include "core/ports/IArtworkReader.h"

class FakeArtworkReader : public shanty::IArtworkReader {
public:
    shanty::Artwork read(const std::string& path) override {
        ++callCount_;
        ++readsPerPath_[path];
        const std::map<std::string, shanty::Artwork>::const_iterator it =
            scripted_.find(path);
        if (it == scripted_.end()) {
            return shanty::Artwork();  // clean miss
        }
        return it->second;
    }

    int readsFor(const std::string& path) const {
        const std::map<std::string, int>::const_iterator it =
            readsPerPath_.find(path);
        return it == readsPerPath_.end() ? 0 : it->second;
    }

    std::map<std::string, shanty::Artwork> scripted_;
    int callCount_ = 0;

private:
    std::map<std::string, int> readsPerPath_;
};
