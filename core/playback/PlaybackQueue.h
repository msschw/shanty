// SH-401: playback queue logic — pure domain (design D2). The audio engine
// adapter only ever receives "play this entry now"; ordering decisions live
// here.
//
// Documented behaviour (tested in tst_playback_queue):
//   - setCurrent/preparation selects the start index (clamped),
//   - next() at the LAST entry does NOT wrap: it returns false and the
//     current index stays on the last entry (stop-at-end decision made in
//     ticket SH-401; wrap-around may be added later as an option),
//   - previous() at the FIRST entry returns false and stays at index 0,
//   - removeCurrent() advances the position to the same index (now holding
//     the following entry) unless the last entry was removed, in which case
//     the position becomes entries.size() (after decrement) or 0 for an
//     empty queue.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace shanty {

class PlaybackQueue {
public:
    /// Replaces the queue contents; entries are plain UTF-8 paths.
    void setEntries(const std::vector<std::string>& paths);
    void clear();

    /// Clamps into [0, size-1]; no-op on an empty queue.
    void setCurrent(std::size_t index);
    bool next();
    bool previous();
    bool jumpTo(std::size_t index);
    void removeCurrent();
    void moveEntry(std::size_t from, std::size_t to);

    std::size_t count() const { return entries_.size(); }
    std::size_t currentIndex() const { return current_; }
    bool empty() const { return entries_.empty(); }
    /// Empty string when the queue is empty.
    std::string current() const;

private:
    std::vector<std::string> entries_;
    std::size_t current_ = 0;
};

}  // namespace shanty
