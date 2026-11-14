// SH-203 Phase A: FilesystemScanner adapter tests on a temp dir fixture.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "adapters/filesystem/FilesystemScanner.h"

using shanty::FilesystemScanner;

namespace {

QStringList toQ(const std::vector<std::string>& paths) {
    QStringList list;
    for (const std::string& p : paths) list << QString::fromStdString(p);
    return list;
}

void touch(const QString& path) {
    QFile file(path);
    QCOMPARE(file.open(QIODevice::WriteOnly), true);
}

}  // namespace

class TestFilesystemScanner : public QObject {
    Q_OBJECT

private slots:
    void returnsSupportedExtensionsOnly();
    void isRecursive();
    void isDeterministicSorted();
    void skipsUnreadableDirectoriesWithoutCrashing();
    void emptyFolderYieldsNothing();
    // SH-1002 (SH-E10 UX-13): playlist enumeration
    void scanPlaylistsReturnsOnlyPlaylistFiles();
    void scanPlaylistsIsRecursive();
    void scanPlaylistsIsDeterministicSortedDeduped();
    void scanPlaylistsNonExistingRootYieldsNothing();
    void audioScanStillIgnoresPlaylistFiles();
};

void TestFilesystemScanner::returnsSupportedExtensionsOnly() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/a.mp3");
    touch(dir.path() + "/b.ogg");
    touch(dir.path() + "/c.flac");
    touch(dir.path() + "/d.txt");
    touch(dir.path() + "/e.mp4");

    FilesystemScanner scanner;
    const QStringList result = toQ(scanner.scan({dir.path().toStdString()}));
    QCOMPARE(result.size(), 3);
    QVERIFY(result.first().endsWith(".mp3"));
}

void TestFilesystemScanner::isRecursive() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/top.mp3");
    QVERIFY(QDir(dir.path()).mkpath("sub"));
    touch(dir.path() + "/sub/inner.flac");

    FilesystemScanner scanner;
    const QStringList result = toQ(scanner.scan({dir.path().toStdString()}));
    QCOMPARE(result.size(), 2);
}

void TestFilesystemScanner::isDeterministicSorted() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/z.mp3");
    touch(dir.path() + "/a.mp3");
    touch(dir.path() + "/m.mp3");

    FilesystemScanner scanner;
    const QStringList first = toQ(scanner.scan({dir.path().toStdString()}));
    const QStringList second = toQ(scanner.scan({dir.path().toStdString()}));
    QCOMPARE(first, second);
    if (first.size() == 3)
        QVERIFY(first.at(0) <= first.at(1) && first.at(1) <= first.at(2));
}

void TestFilesystemScanner::skipsUnreadableDirectoriesWithoutCrashing() {
    // A non-existing root must not crash; it simply contributes nothing.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/a.mp3");

    FilesystemScanner scanner;
    const QStringList result =
        toQ(scanner.scan({dir.path().toStdString(),
                          (dir.path() + QStringLiteral("/gone")).toStdString()}));
    QCOMPARE(result.size(), 1);
}

void TestFilesystemScanner::emptyFolderYieldsNothing() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FilesystemScanner scanner;
    const QStringList result =
        toQ(scanner.scan({dir.path().toStdString()}));
    QCOMPARE(result, QStringList());
}

// ---- SH-1002: playlist enumeration (same contract as scan(), .m3u/.m3u8) ----

void TestFilesystemScanner::scanPlaylistsReturnsOnlyPlaylistFiles() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/top.m3u");
    touch(dir.path() + "/top.m3u8");
    touch(dir.path() + "/song.mp3");
    touch(dir.path() + "/notes.txt");
    touch(dir.path() + "/set.m3u.bak");

    FilesystemScanner scanner;
    const QStringList result =
        toQ(scanner.scanPlaylists({dir.path().toStdString()}));
    QCOMPARE(result.size(), 2);
    QVERIFY(result.contains(dir.path() + QStringLiteral("/top.m3u")));
    QVERIFY(result.contains(dir.path() + QStringLiteral("/top.m3u8")));
}

void TestFilesystemScanner::scanPlaylistsIsRecursive() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/top.m3u");
    QVERIFY(QDir(dir.path()).mkpath("playlists/sub"));
    touch(dir.path() + "/playlists/sub/inner.m3u8");

    FilesystemScanner scanner;
    const QStringList result =
        toQ(scanner.scanPlaylists({dir.path().toStdString()}));
    QCOMPARE(result.size(), 2);
    QVERIFY(result.contains(dir.path() + QStringLiteral("/playlists/sub/inner.m3u8")));
}

void TestFilesystemScanner::scanPlaylistsIsDeterministicSortedDeduped() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/b.m3u");
    touch(dir.path() + "/a.m3u");
    touch(dir.path() + "/sub/c.m3u8");

    FilesystemScanner scanner;
    const QStringList result =
        toQ(scanner.scanPlaylists({dir.path().toStdString(),
                                   dir.path().toStdString()}));
    // Sorted by full path; overlapping roots deduplicate.
    QCOMPARE(result, (QStringList{dir.path() + QStringLiteral("/a.m3u"),
                                  dir.path() + QStringLiteral("/b.m3u"),
                                  dir.path() + QStringLiteral("/sub/c.m3u8")}));
}

void TestFilesystemScanner::scanPlaylistsNonExistingRootYieldsNothing() {
    FilesystemScanner scanner;
    const QStringList result =
        toQ(scanner.scanPlaylists({"/nonexistent/shanty/folder"}));
    QCOMPARE(result, QStringList());
}

void TestFilesystemScanner::audioScanStillIgnoresPlaylistFiles() {
    // Regression pin: the audio enumeration contract is untouched by
    // the playlist feature (Interface Segregation).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    touch(dir.path() + "/song.mp3");
    touch(dir.path() + "/top.m3u");

    FilesystemScanner scanner;
    const QStringList result = toQ(scanner.scan({dir.path().toStdString()}));
    QCOMPARE(result, QStringList{dir.path() + QStringLiteral("/song.mp3")});
}

QTEST_MAIN(TestFilesystemScanner)
#include "tst_filesystem_scanner.moc"
