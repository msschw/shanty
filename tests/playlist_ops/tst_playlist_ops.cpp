// SH-304 Phase A: Playlist mutation semantics.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "domain/Playlist.h"
#include "domain/Track.h"  // for kUnknownDuration

using shanty::Playlist;
using shanty::PlaylistEntry;
using shanty::Track;

namespace {

PlaylistEntry entry(const std::string& path, const std::string& title = "",
                    int duration = Track::kUnknownDuration) {
    PlaylistEntry e;
    e.path = path;
    e.title = title;
    e.durationSeconds = duration;
    return e;
}

}  // namespace

class TestPlaylistOps : public QObject {
    Q_OBJECT

private slots:
    void addAppendsAndCounts();
    void moveEntryReorders();
    void moveEntryNoopOutOfRange();
    void removeAtDeletes();
    void clearEmpties();
    void deduplicateKeepsFirstOccurrence();
    void totalDurationSumsAndFlagsUnknown();
};

void TestPlaylistOps::addAppendsAndCounts() {
    Playlist playlist;
    QCOMPARE(playlist.count(), size_t(0));
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/b.ogg", "B", 90));
    QCOMPARE(playlist.count(), size_t(2));
    QCOMPARE(QString::fromStdString(playlist.entries()[1].title), QString::fromLatin1("B"));
}

void TestPlaylistOps::moveEntryReorders() {
    Playlist playlist;
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/b.ogg"));
    playlist.add(entry("/c.flac"));
    playlist.moveEntry(2, 0);
    QCOMPARE(playlist.count(), size_t(3));
    QCOMPARE(QString::fromStdString(playlist.entries()[0].path), QString::fromLatin1("/c.flac"));
    QCOMPARE(QString::fromStdString(playlist.entries()[2].path), QString::fromLatin1("/b.ogg"));
}

void TestPlaylistOps::moveEntryNoopOutOfRange() {
    Playlist playlist;
    playlist.add(entry("/a.mp3"));
    playlist.moveEntry(5, 0);   // from out of range
    playlist.moveEntry(0, 9);   // to out of range
    playlist.moveEntry(0, 0);   // no-op by definition
    QCOMPARE(playlist.count(), size_t(1));
    if (playlist.count() > 0)
        QCOMPARE(QString::fromStdString(playlist.entries()[0].path), QString::fromLatin1("/a.mp3"));
}

void TestPlaylistOps::removeAtDeletes() {
    Playlist playlist;
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/b.ogg"));
    playlist.removeAt(0);
    QCOMPARE(playlist.count(), size_t(1));
    if (playlist.count() > 0)
        QCOMPARE(QString::fromStdString(playlist.entries()[0].path), QString::fromLatin1("/b.ogg"));
    playlist.removeAt(6);  // no crash, no change
    QCOMPARE(playlist.count(), size_t(1));
}

void TestPlaylistOps::clearEmpties() {
    Playlist playlist;
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/b.ogg"));
    playlist.clear();
    QCOMPARE(playlist.count(), size_t(0));
}

void TestPlaylistOps::deduplicateKeepsFirstOccurrence() {
    Playlist playlist;
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/b.ogg"));
    playlist.add(entry("/a.mp3"));
    playlist.add(entry("/a.mp3"));
    playlist.deduplicate();
    QCOMPARE(playlist.count(), size_t(2));
    QCOMPARE(QString::fromStdString(playlist.entries()[0].path), QString::fromLatin1("/a.mp3"));
    QCOMPARE(QString::fromStdString(playlist.entries()[1].path), QString::fromLatin1("/b.ogg"));
}

void TestPlaylistOps::totalDurationSumsAndFlagsUnknown() {
    Playlist playlist;
    QCOMPARE(playlist.totalDurationSeconds(), 0LL);

    playlist.add(entry("/a.mp3", "", 60));
    playlist.add(entry("/b.ogg", "", 90));
    QCOMPARE(playlist.totalDurationSeconds(), 150LL);

    playlist.add(entry("/c.flac"));  // duration unknown
    QCOMPARE(playlist.totalDurationSeconds(), (long long)Track::kUnknownDuration);
}

QTEST_MAIN(TestPlaylistOps)
#include "tst_playlist_ops.moc"
