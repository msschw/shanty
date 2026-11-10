// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioFormat.h"

namespace shanty {
// TODO(SH-201 Phase B): implement extension classification.
AudioFormat classifyExtension(const std::string&) {
    return AudioFormat::Unsupported;  // unimplemented (tests must fail red)
}

bool isSupportedAudioPath(const std::string&) {
    return false;  // unimplemented (tests must fail red)
}
}  // namespace shanty
