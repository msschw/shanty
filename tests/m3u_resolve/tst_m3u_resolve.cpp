// SH-305 Phase A: playlist entry path resolution at the import boundary.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "domain/PathResolver.h"

#include "domain/Playlist.h"
#include "domain/PlaylistEntry.h"

using shanty::Playlist;
using shanty::PlaylistEntry;
using shanty::resolveEntryPaths;

namespace {
PlaylistEntry entry(const std::string& path) {
    PlaylistEntry e;
    e.path = path;
    return e;
}
/// Indexing must stay inside bounds even while stubs are unimplemented:
/// fail fast when the entry to inspect does not exist.
QString p(const Playlist& resolved, const Playlist& original, int i) {
    if (i >= int(resolved.count()) ||
        resolved.count() != original.count()) {
        QTest::qFail("resolver must keep every entry present and unmutated",
                     __FILE__, __LINE__);
        return QString();
    }
    return QString::fromStdString(resolved.entries()[i].path);
}
}  // namespace

class TestM3uResolve : public QObject {
    Q_OBJECT

private slots:
    void relativeEntriesResolveAgainstBaseDir();
    void absolutePathsPassThrough();
    void fileSchemeUrlsConvertToPlainPaths();
    void urlEncodedCharactersAreDecoded();
    void emptyBaseDirLeavesRelativeEntriesUnchanged();
};

void TestM3uResolve::relativeEntriesResolveAgainstBaseDir() {
    Playlist playlist;
    playlist.add(entry("sub/song.mp3"));
    playlist.add(entry("./other.flac"));
    const Playlist resolved = resolveEntryPaths(playlist, "/music/lists");
    QCOMPARE(p(resolved, playlist, 0), QString::fromLatin1("/music/lists/sub/song.mp3"));
    QCOMPARE(p(resolved, playlist, 1), QString::fromLatin1("/music/lists/other.flac"));
}

void TestM3uResolve::absolutePathsPassThrough() {
    Playlist playlist;
    playlist.add(entry("/music/absolute.mp3"));
    const Playlist resolved = resolveEntryPaths(playlist, "/music/lists");
    QCOMPARE(p(resolved, playlist, 0), QString::fromLatin1("/music/absolute.mp3"));
}

void TestM3uResolve::fileSchemeUrlsConvertToPlainPaths() {
    Playlist playlist;
    playlist.add(entry("file:///music/plain%20name.mp3"));
    playlist.add(entry("file://localhost/music/local.mp3"));
    const Playlist resolved = resolveEntryPaths(playlist, "/music/lists");
    QCOMPARE(p(resolved, playlist, 0), QString::fromLatin1("/music/plain name.mp3"));
    QCOMPARE(p(resolved, playlist, 1), QString::fromLatin1("/music/local.mp3"));
}

void TestM3uResolve::urlEncodedCharactersAreDecoded() {
    Playlist playlist;
    playlist.add(entry("file:///music/fj%C3%B6rd.mp3"));
    const Playlist resolved = resolveEntryPaths(playlist, "/music/lists");
    QCOMPARE(p(resolved, playlist, 0), QString::fromUtf8("/music/fjördmp3").replace("ördmp3", "örd.mp3"));
}

void TestM3uResolve::emptyBaseDirLeavesRelativeEntriesUnchanged() {
    Playlist playlist;
    playlist.add(entry("song.mp3"));
    const Playlist resolved = resolveEntryPaths(playlist, "");
    QCOMPARE(p(resolved, playlist, 0), QString::fromLatin1("song.mp3"));
}

QTEST_MAIN(TestM3uResolve)
#include "tst_m3u_resolve.moc"
