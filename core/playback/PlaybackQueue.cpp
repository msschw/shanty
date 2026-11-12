// SH-401 Phase B: PlaybackQueue implementation (pure domain logic, D2).
// Queue ordering decisions live here; the audio engine adapter only ever
// receives "play this entry now". Edges stop: next() at the last entry and
// previous() at the first entry both return false and leave the position
// unchanged (no wrap-around, per the SH-401 decision).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaybackQueue.h"

namespace shanty {

void PlaybackQueue::setEntries(const std::vector<std::string>& paths) {
    entries_ = paths;
    current_ = 0;
}

void PlaybackQueue::clear() {
    entries_.clear();
    current_ = 0;
}

void PlaybackQueue::setCurrent(std::size_t index) {
    if (entries_.empty()) {
        return;  // no-op on an empty queue (documented contract)
    }
    if (index < entries_.size()) {
        current_ = index;
    } else {
        current_ = entries_.size() - 1;  // clamp into [0, size-1]
    }
}

bool PlaybackQueue::next() {
    if (entries_.empty() || current_ >= entries_.size() - 1) {
        return false;  // stop-at-end: no wrap-around
    }
    ++current_;
    return true;
}

bool PlaybackQueue::previous() {
    if (entries_.empty() || current_ == 0) {
        return false;  // stay at the first entry
    }
    --current_;
    return true;
}

bool PlaybackQueue::jumpTo(std::size_t index) {
    if (index >= entries_.size()) {
        return false;  // out of range: rejected, selection unchanged
    }
    current_ = index;
    return true;
}

void PlaybackQueue::removeCurrent() {
    if (entries_.empty()) {
        return;  // no-op on an empty queue
    }
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(current_));
    if (current_ >= entries_.size()) {
        // The removed entry had no follower: the position retreats onto the
        // new last entry, or 0 for an emptied queue.
        current_ = entries_.empty() ? 0 : entries_.size() - 1;
    }
    // Otherwise the position stays on the same index, which now holds the
    // following entry.
}

void PlaybackQueue::moveEntry(std::size_t from, std::size_t to) {
    if (from >= entries_.size() || from == to) {
        return;
    }
    // `to` is used as an insert-at index after erasing `from`, so it is
    // clamped into [0, size-1]; clamping it onto `from` makes the move a
    // no-op.
    const std::size_t last = entries_.size() - 1;
    const std::size_t target = to <= last ? to : last;
    if (target == from) {
        return;
    }

    const std::string entry = entries_[from];
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(from));
    entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(target),
                    entry);

    // Keep the current entry selected under its new index. Indices shift
    // with erase-then-insert: after erasing `from`, anything above it moves
    // down one; inserting at `target` moves anything at/after it back up.
    if (current_ == from) {
        current_ = target;
    } else {
        if (current_ > from) {
            --current_;
        }
        if (current_ >= target) {
            ++current_;
        }
    }
}

std::string PlaybackQueue::current() const {
    if (entries_.empty() || current_ >= entries_.size()) {
        return std::string();  // empty string when the queue is empty
    }
    return entries_[current_];
}

const std::vector<std::string>& PlaybackQueue::entries() const {
    // SH-805: the persistence snapshot source — the live stored paths in
    // queue order (reference, no copy on read).
    return entries_;
}

}  // namespace shanty
