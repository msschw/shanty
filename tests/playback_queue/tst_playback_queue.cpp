// SH-401 Phase A: PlaybackQueue semantics (stop-at-end, no wrap-around).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "playback/PlaybackQueue.h"

using shanty::PlaybackQueue;

class TestPlaybackQueue : public QObject {
    Q_OBJECT

private slots:
    void setEntriesStartsAtFirst();
    void nextWalksForward();
    void nextAtLastEntryDoesNotWrap();
    void previousAtFirstEntryDoesNotGoNegative();
    void jumpToSelectsIndex();
    void jumpToOutOfRangeIsRejected();
    void removeCurrentAdvancesToSameIndex();
    void removeLastEntryRetreats();
    void removeCurrentOnEmptyQueueIsNoop();
    void moveEntryReordersWhilePlaying();
    void clearResets();

    // ---- SH-805: entries snapshot accessor ---------------------------------
    void entriesReturnsStoredPathsInOrder();
};

void TestPlaybackQueue::setEntriesStartsAtFirst() {
    PlaybackQueue queue;
    queue.setEntries({"/a.mp3", "/b.ogg", "/c.flac"});
    QCOMPARE(queue.currentIndex(), size_t(0));
    QCOMPARE(QString::fromStdString(queue.current()), QString::fromLatin1("/a.mp3"));
}

void TestPlaybackQueue::nextWalksForward() {
    PlaybackQueue queue;
    queue.setEntries({"/a.mp3", "/b.ogg"});
    QVERIFY(queue.next());
    QCOMPARE(queue.currentIndex(), size_t(1));
}

void TestPlaybackQueue::nextAtLastEntryDoesNotWrap() {
    PlaybackQueue queue;
    queue.setEntries({"/a.mp3", "/b.ogg"});
    queue.next();
    QVERIFY(!queue.next());  // stop-at-end: no wrap-around
    QCOMPARE(queue.currentIndex(), size_t(1));
}

void TestPlaybackQueue::previousAtFirstEntryDoesNotGoNegative() {
    PlaybackQueue queue;
    queue.setEntries({"/a.mp3", "/b.ogg"});
    QVERIFY(!queue.previous());
    QCOMPARE(queue.currentIndex(), size_t(0));
}

void TestPlaybackQueue::jumpToSelectsIndex() {
    PlaybackQueue queue;
    queue.setEntries({"/a", "/b", "/c"});
    QVERIFY(queue.jumpTo(2));
    QCOMPARE(QString::fromStdString(queue.current()), QString::fromLatin1("/c"));
}

void TestPlaybackQueue::jumpToOutOfRangeIsRejected() {
    PlaybackQueue queue;
    queue.setEntries({"/a", "/b"});
    QVERIFY(!queue.jumpTo(2));
    QCOMPARE(queue.currentIndex(), size_t(0));
}

void TestPlaybackQueue::removeCurrentAdvancesToSameIndex() {
    PlaybackQueue queue;
    queue.setEntries({"/a", "/b", "/c"});
    queue.jumpTo(1);  // "/b"
    queue.removeCurrent();
    QCOMPARE(queue.count(), size_t(2));
    QCOMPARE(queue.currentIndex(), size_t(1));
    QCOMPARE(QString::fromStdString(queue.current()), QString::fromLatin1("/c"));
}

void TestPlaybackQueue::removeLastEntryRetreats() {
    PlaybackQueue queue;
    queue.setEntries({"/a", "/b"});
    queue.next();  // index 1 = "/b"
    queue.removeCurrent();
    QCOMPARE(queue.count(), size_t(1));
    QCOMPARE(queue.currentIndex(), size_t(0));
    QCOMPARE(QString::fromStdString(queue.current()), QString::fromLatin1("/a"));
}

void TestPlaybackQueue::removeCurrentOnEmptyQueueIsNoop() {
    PlaybackQueue queue;
    queue.removeCurrent();
    QCOMPARE(queue.count(), size_t(0));
}

void TestPlaybackQueue::moveEntryReordersWhilePlaying() {
    PlaybackQueue queue;
    queue.setEntries({"/a", "/b", "/c"});
    queue.jumpTo(2);  // playing "/c"
    queue.moveEntry(2, 0);
    QCOMPARE(queue.currentIndex(), size_t(0));
    QCOMPARE(QString::fromStdString(queue.current()), QString::fromLatin1("/c"));
}

void TestPlaybackQueue::clearResets() {
    PlaybackQueue queue;
    queue.setEntries({"/a", "/b"});
    queue.next();
    queue.clear();
    QVERIFY(queue.empty());
    QCOMPARE(queue.currentIndex(), size_t(0));
    QCOMPARE(QString::fromStdString(queue.current()), QString());
}

void TestPlaybackQueue::entriesReturnsStoredPathsInOrder() {
    PlaybackQueue queue;
    queue.setEntries({"/a.mp3", "/b.ogg", "/c.flac"});

    // SH-805: the persistence snapshot source — the stored paths, queue
    // order, reference to the live storage (no copy on read).
    QCOMPARE(queue.entries().size(), size_t(3));
    QCOMPARE(QString::fromStdString(queue.entries()[0]),
             QString::fromLatin1("/a.mp3"));
    QCOMPARE(QString::fromStdString(queue.entries()[1]),
             QString::fromLatin1("/b.ogg"));
    QCOMPARE(QString::fromStdString(queue.entries()[2]),
             QString::fromLatin1("/c.flac"));

    // Empty queue → empty list.
    queue.clear();
    QCOMPARE(queue.entries().size(), size_t(0));
}

QTEST_MAIN(TestPlaybackQueue)
#include "tst_playback_queue.moc"
