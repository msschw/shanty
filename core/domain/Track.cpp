// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "Track.h"

#include <cstddef>

namespace shanty {
// TODO(SH-201 Phase B): implement construction rules:
//  - title falls back to the basename without extension,
//  - duration unknown until metadata is read,
//  - unsupported extensions produce an invalid Track.
Track Track::make(const std::string&) {
    return Track();  // unimplemented (tests must fail red)
}
}  // namespace shanty
