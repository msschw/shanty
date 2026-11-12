// SH-805 Phase A: QueueStateStore — the playback-session persistence
// (queue paths, selected index, position seconds, transport state; UX-4).
// The store follows the FileIndexStore (SH-204) idiom: explicit-path or
// env-override location, plain UTF-8 text format, atomic writes, and a
// corrupt/truncated file yielding a clean empty restore. The debounce
// contract (trailing-write coalescing, flush, dtor flush) is asserted
// deterministically through the debounceMs parameter (0 = immediate write,
// a huge window = coalesce until flush).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "adapters/store/QueueStateStore.h"

using shanty::QueueState;
using shanty::QueueStateStore;

namespace {

/// A scratch file per test: no default-location pollution.
class ScratchPath {
public:
    ScratchPath() : dir_(QDir::tempPath() + QStringLiteral("/shanty-tst-queue-store")) {
        QDir().mkpath(dir_);
        file_ = dir_ + QStringLiteral("/queue-%1.conf")
                             .arg(QRandomGenerator::system()->generate());
    }
    ~ScratchPath() {
        QFile::remove(file_);
        QDir(dir_).removeRecursively();
    }
    QString path() const { return file_; }

private:
    QString dir_;
    QString file_;
};

QueueState sampleState() {
    QueueState state;
    state.paths = {"/music/a.mp3", "/music/b.ogg", "/music/c.flac"};
    state.index = 1;
    state.positionSeconds = 42;
    state.transportState = "paused";
    return state;
}

}  // namespace

class TestQueueStore : public QObject {
    Q_OBJECT

private slots:
    // ---- round trip + clean restore ---------------------------------------
    void saveThenLoadRoundTrips();
    void loadOnMissingFileReturnsEmptyState();
    void corruptFileYieldsEmptyState();
    void truncatedFileYieldsEmptyState();

    // ---- debounce (trailing-write coalescing) ------------------------------
    void firstSaveWritesImmediately();
    void savesInsideWindowCoalesceIntoLatestPending();
    void flushWritesPendingImmediately();
    void saveOutsideWindowWritesLatestSnapshot();
};

void TestQueueStore::saveThenLoadRoundTrips() {
    ScratchPath scratch;
    QueueStateStore store(scratch.path().toStdString(), 0);

    const QueueState state = sampleState();
    store.save(state);

    QVERIFY(state == store.load());
}

void TestQueueStore::loadOnMissingFileReturnsEmptyState() {
    ScratchPath scratch;
    QueueStateStore store(scratch.path().toStdString(), 0);

    QVERIFY(QueueState() == store.load());
}

void TestQueueStore::corruptFileYieldsEmptyState() {
    ScratchPath scratch;
    QFile file(scratch.path());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("\x01\x02garbage\xff not a queue file\n\n\x01", 30);
    file.close();

    QueueStateStore store(scratch.path().toStdString(), 0);
    QVERIFY(QueueState() == store.load());
}

void TestQueueStore::truncatedFileYieldsEmptyState() {
    ScratchPath scratch;
    // A valid-looking header but the required tags are cut off: the restore
    // must be clean and empty (no partial adoption, no crash).
    QFile file(scratch.path());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("# shanty queue state v1\nstate=pau");
    file.close();

    QueueStateStore store(scratch.path().toStdString(), 0);
    QVERIFY(QueueState() == store.load());
}

void TestQueueStore::firstSaveWritesImmediately() {
    ScratchPath scratch;
    // No previous write ever happened: the very first save must hit the disk
    // right away (the restore path must find data even after a quick kill).
    QueueStateStore store(scratch.path().toStdString(), 60000);

    store.save(sampleState());
    QVERIFY(sampleState() == store.load());
}

void TestQueueStore::savesInsideWindowCoalesceIntoLatestPending() {
    ScratchPath scratch;
    QueueStateStore store(scratch.path().toStdString(), 60000);

    store.save(sampleState());  // first save → immediate write

    QueueState later = sampleState();
    later.index = 2;
    later.positionSeconds = 3;
    later.transportState = "playing";
    store.save(later);  // inside the window → coalesced, NOT written

    QVERIFY(sampleState() == store.load());  // disk still holds the first
}

void TestQueueStore::flushWritesPendingImmediately() {
    ScratchPath scratch;
    QueueStateStore store(scratch.path().toStdString(), 60000);

    store.save(sampleState());
    QueueState later = sampleState();
    later.index = 2;
    later.positionSeconds = 3;
    store.save(later);  // coalesced into pending

    store.flush();  // documented shutdown hook
    QVERIFY(later == store.load());
}

void TestQueueStore::saveOutsideWindowWritesLatestSnapshot() {
    ScratchPath scratch;
    // debounceMs = 0: every save is outside the window → every save writes
    // (the position-tick cadence case; the store always ends up with the
    // latest snapshot).
    QueueStateStore store(scratch.path().toStdString(), 0);

    QueueState first = sampleState();
    store.save(first);
    QVERIFY(first == store.load());

    first.index = 2;
    first.positionSeconds = 7;
    store.save(first);
    QVERIFY(first == store.load());
}

QTEST_MAIN(TestQueueStore)
#include "tst_queue_store.moc"
