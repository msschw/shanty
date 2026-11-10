// SH-204 Phase A: FileIndexStore round trip.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "adapters/store/FileIndexStore.h"

using shanty::FileIndexStore;
using shanty::LibraryConfig;

class TestLibraryStore : public QObject {
    Q_OBJECT

private slots:
    void saveThenLoadRoundTrips();
    void loadOnFreshStoreReturnsEmptyConfig();
    void saveOverwritesPreviousConfig();
};

void TestLibraryStore::saveThenLoadRoundTrips() {
    LibraryConfig config;
    config.folders = {"/music", "/podcast-archiv"};
    config.whitelist = {"/music/fav"};
    config.blacklist = {"/music/skip"};

    FileIndexStore store;
    store.save(config);

    QVERIFY(config == store.load());
}

void TestLibraryStore::loadOnFreshStoreReturnsEmptyConfig() {
    FileIndexStore store;
    QVERIFY(LibraryConfig() == store.load());
}

void TestLibraryStore::saveOverwritesPreviousConfig() {
    FileIndexStore store;

    LibraryConfig first;
    first.folders = {"/music"};
    store.save(first);

    LibraryConfig second;
    second.folders = {"/new-music"};
    second.whitelist = {"/new-music/hits"};
    store.save(second);

    QVERIFY(second == store.load());
    QVERIFY(!(first == store.load()));
}

QTEST_MAIN(TestLibraryStore)
#include "tst_library_store.moc"
