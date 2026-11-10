// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaybackQueue.h"

namespace shanty {
// TODO(SH-401 Phase B): implement the documented queue semantics
// (no wrap-around at the edges per the SH-401 decision).

void PlaybackQueue::setEntries(const std::vector<std::string>&) {}  // unimplemented (tests must fail red)
void PlaybackQueue::clear() {}                                     // unimplemented (tests must fail red)
void PlaybackQueue::setCurrent(std::size_t) {}                     // unimplemented (tests must fail red)
bool PlaybackQueue::next() { return false; }                       // unimplemented (tests must fail red)
bool PlaybackQueue::previous() { return false; }                   // unimplemented (tests must fail red)
bool PlaybackQueue::jumpTo(std::size_t) { return false; }          // unimplemented (tests must fail red)
void PlaybackQueue::removeCurrent() {}                             // unimplemented (tests must fail red)
void PlaybackQueue::moveEntry(std::size_t, std::size_t) {}         // unimplemented (tests must fail red)
std::string PlaybackQueue::current() const { return std::string(); }

}  // namespace shanty
