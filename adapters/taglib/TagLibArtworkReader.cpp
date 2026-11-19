// SH-1009 Phase A (UX-20): stub of the embedded-artwork reader — the
// Phase B implementation lands after the red test run. Clean miss by
// contract; replaced by the taglib extraction below.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "TagLibArtworkReader.h"

namespace shanty {

Artwork TagLibArtworkReader::read(const std::string& path) {
    (void)path;
    return Artwork();  // stub: Phase B extracts the embedded picture
}

}  // namespace shanty
