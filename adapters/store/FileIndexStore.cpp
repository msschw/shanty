// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "FileIndexStore.h"

namespace shanty {
// TODO(SH-204 Phase B): persist to the Sailfish sandbox settings file.
// The port stays format-agnostic in the tests (see design decision D5/D1).

void FileIndexStore::save(const LibraryConfig&) {}  // unimplemented (tests must fail red)

LibraryConfig FileIndexStore::load() {
    return LibraryConfig();  // unimplemented: always-empty (tests must fail red)
}
}  // namespace shanty
