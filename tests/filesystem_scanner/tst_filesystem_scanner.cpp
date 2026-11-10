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

QTEST_MAIN(TestFilesystemScanner)
#include "tst_filesystem_scanner.moc"
