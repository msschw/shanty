// SH-805 Phase A stub: the queue-state persistence contract is locked
// red-first in tests/queue_store; every target case must fail by assertion
// until Phase B implements the real file-backed store.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "QueueStateStore.h"

#include <utility>

namespace shanty {

QueueStateStore::QueueStateStore(std::string filePath, int debounceMs)
    : path_(filePath.empty() ? std::string() : std::move(filePath)),
      debounceMs_(debounceMs) {
    (void)path_;
    (void)debounceMs_;
}

QueueStateStore::~QueueStateStore() {}

void QueueStateStore::save(const QueueState& state) {
    (void)state;
    // Phase A stub: no persistence yet.
}

QueueState QueueStateStore::load() const {
    // Phase A stub: nothing is ever persisted, so every load is empty.
    return QueueState();
}

void QueueStateStore::flush() {}

void QueueStateStore::writePending() {}

}  // namespace shanty
