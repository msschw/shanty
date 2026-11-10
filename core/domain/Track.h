// SH-201: Track value type (design D1 — plain path in UTF-8, no QUrl).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "AudioFormat.h"

namespace shanty {

/// A playable audio track on local storage. Invalid until make() succeeded.
class Track {
public:
    static const int kUnknownDuration = -1;

    /// Constructs a Track from an absolute or relative plain path.
    /// Returns a Track whose isValid() == false when the classification
    /// yields an unsupported format.
    static Track make(const std::string& path);

    const std::string& path() const { return path_; }
    const std::string& title() const { return title_; }
    int durationSeconds() const { return durationSeconds_; }
    AudioFormat format() const { return format_; }
    bool isValid() const { return format_ != AudioFormat::Unsupported; }

private:
    Track() = default;
    std::string path_;
    std::string title_;
    int durationSeconds_ = kUnknownDuration;
    AudioFormat format_ = AudioFormat::Unsupported;
};

}  // namespace shanty
