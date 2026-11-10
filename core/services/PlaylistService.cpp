// SH-603 Phase B: application-layer playlist facade implementation. See
// PlaylistService.h for the documented import/export/mutation contracts
// (error surfacing incl. the unreadable-file case, unconditional export
// overwrite, "mutated → then changed" ordering).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "PlaylistService.h"

#include <fstream>
#include <iterator>

#include "domain/PathResolver.h"

namespace shanty {
namespace {

/// Parent directory of `filePath` by plain string slicing: everything
/// before the LAST '/'; "" when the path carries no '/' (relative entries
/// then stay untouched per the PathResolver baseDir-"" rule). No Qt (D1).
std::string parentDirOf(const std::string& filePath) {
    const std::size_t slash = filePath.rfind('/');
    if (slash == std::string::npos) return std::string();
    return filePath.substr(0, slash);
}

/// Reads the whole file as bytes. False when the file cannot be opened.
bool readFileBytes(const std::string& filePath, std::string& data) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    data.assign(std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());
    return true;
}

/// Writes `data` as bytes (binary, truncate). False when the target cannot
/// be opened or the write reports an error.
bool writeFileBytes(const std::string& filePath, const std::string& data) {
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    return file.good();
}

}  // namespace

PlaylistService::PlaylistService(IPlaylistCodec& codec) : codec_(codec) {}

void PlaylistService::import(const std::string& filePath) {
    ImportOutcome outcome;
    outcome.status = ImportOutcome::Status::ParseErrors;
    outcome.sourceFilePath = filePath;

    // 1) Unreadable file: service-level error (ParseError.Code has no such
    //    member and M3uParseResult.h is frozen); the codec is not called.
    std::string data;
    if (!readFileBytes(filePath, data)) {
        outcome.status = ImportOutcome::Status::UnreadableFile;
        outcome.errorMessage = filePath + ": cannot open for reading";
        if (onImportFinished) onImportFinished(outcome);
        return;
    }

    // 2) Codec parse: codec errors surface verbatim; the current playlist
    //    is NOT applied and no change notification fires.
    const ParseResult parsed = codec_.parse(data, filePath);
    if (!parsed.ok()) {
        outcome.errors = parsed.errors;
        if (!parsed.errors.empty()) outcome.errorMessage = parsed.errors.front().message;
        if (onImportFinished) onImportFinished(outcome);
        return;
    }

    // 3) SH-305/D3: resolve entry paths against the playlist's own
    //    directory at the import boundary; the domain receives resolved
    //    plain paths only.
    outcome.playlist = resolveEntryPaths(parsed.playlist, parentDirOf(filePath));

    // 4) Success: replace the playlist, then fire the observation hooks in
    //    the documented order (import-finished first, then changed with the
    //    final state already visible).
    outcome.status = ImportOutcome::Status::Success;
    playlist_ = outcome.playlist;
    if (onImportFinished) onImportFinished(outcome);
    if (onPlaylistChanged) onPlaylistChanged();
}

void PlaylistService::exportPlaylist(const std::string& filePath) {
    ExportOutcome outcome;
    outcome.status = ExportOutcome::Status::SerializeError;
    outcome.targetFilePath = filePath;

    const SerializeResult serialized = codec_.serialize(playlist_, filePath);
    if (serialized.status != SerializeResult::Status::Success) {
        outcome.errorMessage = serialized.errorMessage;
        if (onExportFinished) onExportFinished(outcome);
        return;
    }
    if (!writeFileBytes(filePath, serialized.data)) {
        outcome.status = ExportOutcome::Status::UnwritableFile;
        outcome.errorMessage = filePath + ": cannot open for writing";
        if (onExportFinished) onExportFinished(outcome);
        return;
    }

    // Existing targets were truncated/overwritten unconditionally (see
    // header decision); the playlist itself is never mutated by export.
    outcome.status = ExportOutcome::Status::Success;
    if (onExportFinished) onExportFinished(outcome);
}

// ---- SH-304 mutation passthrough: mutate first, notify once after --------
void PlaylistService::add(const PlaylistEntry& entry) {
    playlist_.add(entry);
    if (onPlaylistChanged) onPlaylistChanged();
}

void PlaylistService::moveEntry(std::size_t from, std::size_t to) {
    playlist_.moveEntry(from, to);
    if (onPlaylistChanged) onPlaylistChanged();
}

void PlaylistService::removeAt(std::size_t index) {
    playlist_.removeAt(index);
    if (onPlaylistChanged) onPlaylistChanged();
}

void PlaylistService::clear() {
    playlist_.clear();
    if (onPlaylistChanged) onPlaylistChanged();
}

void PlaylistService::deduplicate() {
    playlist_.deduplicate();
    if (onPlaylistChanged) onPlaylistChanged();
}

// ---- observation passthrough ---------------------------------------------
std::size_t PlaylistService::count() const { return playlist_.count(); }

const std::vector<PlaylistEntry>& PlaylistService::entries() const {
    return playlist_.entries();
}

long long PlaylistService::totalDurationSeconds() const {
    return playlist_.totalDurationSeconds();
}

}  // namespace shanty
