// SH-603: application-layer playlist facade. Per ISSUES.md §1.1 the
// PlaylistService belongs to the application-services layer; it stays pure
// C++/Qt-free (design D1) and wires the IPlaylistCodec port (SH-301..303),
// the import-boundary path resolution (SH-305, design D3) and the Playlist
// domain mutations (SH-304) so the view model has ONE facade.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Documented decisions (SH-603; encoded in tst_playlist_vm):
//   - Import (synchronous service; results surface through callbacks —
//     exceptions are never used for flow control, PlaybackController
//     observation-hook pattern):
//       import(filePath):
//         1. Read the file bytes (std::ifstream, binary). An unreadable
//            file surfaces as Status::UnreadableFile WITHOUT calling the
//            codec. (ParseError.Code has no unreadable-file member and
//            M3uParseResult.h is a frozen green foundation, so the
//            service-level outcome status carries this case; the
//            errorMessage is "<path>: cannot open for reading".)
//         2. codec.parse(data, filePath). Any codec error →
//            Status::ParseErrors carrying the codec's error list verbatim;
//            the current playlist stays UNCHANGED and onPlaylistChanged
//            does NOT fire.
//         3. SH-305 / D3: the service applies path resolution at the
//            import boundary — resolveEntryPaths(parsed, baseDir) with
//            baseDir = the playlist file's parent directory, derived from
//            filePath by plain string slicing (everything before the LAST
//            '/'; "" when filePath carries no '/', which leaves relative
//            entries untouched per the PathResolver rule). The domain
//            receives resolved plain paths only; metadata (title,
//            duration) is carried through unchanged.
//         4. Success → playlist_ is replaced by the resolved playlist and
//            the callbacks fire in this order: onImportFinished(outcome),
//            then onPlaylistChanged (observers of the change already see
//            the final state at callback time).
//       All callbacks fire before import() returns; a later async
//       file-dialog layer keeps the same observation contract.
//   - Export (also documents the overwrite semantics):
//       exportPlaylist(filePath): codec.serialize(playlist_, filePath) →
//       Status::SerializeError (codec's errorMessage forwarded) when the
//       codec rejects; otherwise the serialized bytes are written with
//       std::ofstream (binary, truncate). An existing target file is
//       OVERWRITTEN UNCONDITIONALLY: overwrite confirmation belongs to the
//       dialog flow (the view model's file picker decides the target
//       before confirmExport — see PlaylistViewModel.h), the service stays
//       confirm-free. A target that cannot be opened/written →
//       Status::UnwritableFile, errorMessage "<path>: cannot open for
//       writing". exportPlaylist NEVER mutates the playlist.
//   - Mutation passthrough (SH-304): add/moveEntry/removeAt/clear/
//     deduplicate each call the domain exactly once and fire
//     onPlaylistChanged exactly once AFTER the mutation was applied —
//     including for domain no-ops (out-of-range index, dedupe with nothing
//     to drop, clear of an empty playlist). The ordering guarantee is
//     therefore "mutated → then changed": the callback always observes the
//     already-updated state, and uniform firing spares observers from
//     change-detection heuristics.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "domain/M3uParseResult.h"
#include "domain/Playlist.h"
#include "domain/PlaylistEntry.h"
#include "ports/IPlaylistCodec.h"

namespace shanty {

/// Result of import(): a value carrying either the resolved playlist or
/// the failure reason (codec errors verbatim, or the unreadable-file case
/// that only the service can detect).
struct ImportOutcome {
    enum class Status {
        Success,       // parsed, resolved, applied as the current playlist
        UnreadableFile, // the file could not be opened for reading
        ParseErrors,   // the codec reported errors; playlist NOT applied
    };

    Status status = Status::Success;
    /// The resolved playlist (SH-305/D3); meaningful when Success.
    Playlist playlist;
    /// Codec errors verbatim when Status::ParseErrors, else empty.
    std::vector<ParseError> errors;
    std::string sourceFilePath;  // as given to import()
    /// Human-readable failure message for the UI; "" on Success.
    std::string errorMessage;
};

/// Result of export(): where the serialized bytes went, or why not.
struct ExportOutcome {
    enum class Status {
        Success,        // serialized and written to disk
        SerializeError, // the codec rejected the playlist
        UnwritableFile, // target could not be opened/written
    };

    Status status = Status::Success;
    std::string targetFilePath;  // as given to exportPlaylist()
    /// Human-readable failure message for the UI; "" on Success.
    std::string errorMessage;
};

class PlaylistService {
public:
    explicit PlaylistService(IPlaylistCodec& codec);

    // ---- observation hooks -------------------------------------------------
    /// Fired after a mutation delegate was applied and after a SUCCESSFUL
    /// import replaced the playlist (never on a failed import; for export
    /// see the doc above — exportPlaylist never changes the playlist).
    std::function<void()> onPlaylistChanged;
    /// Fired exactly once per import() with the final ImportOutcome.
    std::function<void(const ImportOutcome&)> onImportFinished;
    /// Fired exactly once per exportPlaylist() with the final ExportOutcome.
    std::function<void(const ExportOutcome&)> onExportFinished;

    // ---- import / export (SH-303 wiring; see header decisions) -------------
    void import(const std::string& filePath);
    void exportPlaylist(const std::string& filePath);

    // ---- SH-304 mutation passthrough (ONE facade for the view model) -------
    void add(const PlaylistEntry& entry);
    /// Moves the entry at `from` to index `to` (SH-304 semantics: out of
    /// range / from == to is a domain no-op; the change hook still fires).
    void moveEntry(std::size_t from, std::size_t to);
    /// Removes the entry at `index` (out of range: domain no-op).
    void removeAt(std::size_t index);
    void clear();
    /// Removes later duplicates of the same resolved path, keeping the
    /// first occurrence (SH-304 semantics).
    void deduplicate();

    // ---- playlist observation (passthrough) -------------------------------
    std::size_t count() const;
    const std::vector<PlaylistEntry>& entries() const;
    long long totalDurationSeconds() const;

private:
    IPlaylistCodec& codec_;
    Playlist playlist_;
};

}  // namespace shanty
