// Wave-8 follow-up: FilePickerPage directory-listing contract
// (app/controllers/PickerSupport.h): filter semantics must match the QML
// layer's documented suffix rule (case-insensitive "endsWith", dirs always
// admitted, empty filter admits everything).
// Phase A red reason: pickers::listDirectory passed the suffixes straight
// to QDir::entryInfoList, which treats nameFilters as GLOB patterns —
// ".m3u" matches no real file, so playlist import saw only directories.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include "app/controllers/PickerSupport.h"

using shanty::pickers::listDirectory;

namespace {

/// A listing row map by its "name" value ("" when absent).
QVariantMap row(const QVariantList& listing, const QString& name) {
    for (const QVariant& entry : listing) {
        const QVariantMap item = entry.toMap();
        if (item.value("name").toString() == name)
            return item;
    }
    return QVariantMap();
}

QStringList names(const QVariantList& listing) {
    QStringList out;
    for (const QVariant& entry : listing)
        out << entry.toMap().value("name").toString();
    return out;
}

/// touch(path) — creates an empty file, making every parent directory.
bool touch(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           (f.close(), true);
}

}  // namespace

class TestPickerSupport : public QObject {
    Q_OBJECT

private slots:
    void suffixFiltersAdmitFilesCaseInsensitivelyAndKeepDirs();
    void emptyFilterAdmitsEveryFile();
    void unreadableDirectoryYieldsEmptyList();
    void rowsCarryNamePathAndIsDirSortedDirsFirst();

private:
    /// Shared fixture: dir/ with jolla/ subdir, m3u/m3u8/txt files and a
    /// case-shifted .M3U file.
    QTemporaryDir dir;
    QString sub;
    QString plain;
    QString mixed;
    QString upper;

    void init() {
        QVERIFY(dir.isValid());
        sub = dir.path() + "/jolla";
        plain = dir.path() + "/Song.m3u8";
        mixed = dir.path() + "/lower.m3u";
        upper = dir.path() + "/TRACK.M3U";
        QVERIFY(QDir().mkpath(sub));
        QVERIFY(touch(dir.path() + "/notes.txt"));
        QVERIFY(touch(plain));
        QVERIFY(touch(mixed));
        QVERIFY(touch(upper));
        QVERIFY(touch(sub + "/a.ogg"));
    }
};

void TestPickerSupport::suffixFiltersAdmitFilesCaseInsensitivelyAndKeepDirs() {
    init();
    const QVariantList listing = listDirectory(
        dir.path(), QVariantList{".m3u", ".m3u8"});

    // Directories are always listed (navigation must keep working).
    QVERIFY2(names(listing).contains("jolla"),
             "directories must be listed regardless of name filters");
    // Suffix semantics, case-insensitive endsWith (QML contract parity):
    // Song.m3u8 and lower.m3u and TRACK.M3U all pass, notes.txt does not.
    QVERIFY2(names(listing).contains("Song.m3u8"),
             ".m3u8 suffix must be admitted");
    QVERIFY2(names(listing).contains("lower.m3u"),
             ".m3u suffix must be admitted");
    QVERIFY2(names(listing).contains("TRACK.M3U"),
             "suffix matching must be case-insensitive (QML parity)");
    QVERIFY2(!names(listing).contains("notes.txt"),
             "non-matching files must stay out of the listing");
    QVERIFY2(!names(listing).contains("a.ogg"),
             "files in subdirectories are not part of the parent listing");
}

void TestPickerSupport::emptyFilterAdmitsEveryFile() {
    init();
    const QVariantList listing = listDirectory(dir.path(), QVariantList{});
    QVERIFY(names(listing).contains("jolla"));
    QVERIFY(names(listing).contains("notes.txt"));
    QVERIFY(names(listing).contains("Song.m3u8"));
    QVERIFY(names(listing).contains("TRACK.M3U"));
}

void TestPickerSupport::unreadableDirectoryYieldsEmptyList() {
    init();
    QVERIFY(listDirectory(dir.path(), {}).isEmpty() == false);  // sanity
    QFile::Permissions const keep =
        QFileInfo(dir.path()).permissions();
    QVERIFY(QFile::setPermissions(dir.path(), QFile::WriteOwner));
    const QVariantList listing = listDirectory(dir.path(), {});
    QVERIFY2(listing.isEmpty(),
             "an unreadable directory yields the documented empty list");
    QFile::setPermissions(dir.path(), keep);  // restore for QTemporaryDir
}

void TestPickerSupport::rowsCarryNamePathAndIsDirSortedDirsFirst() {
    init();
    const QVariantList listing = listDirectory(dir.path(), {});
    QVERIFY(!listing.isEmpty());
    // Directories first, then name-sorted files.
    QCOMPARE(listing.first().toMap().value("isDir").toBool(), true);
    QCOMPARE(listing.first().toMap().value("name").toString(),
             QString("jolla"));
    const QVariantMap song = row(listing, "Song.m3u8");
    QCOMPARE(song.value("path").toString(),
             dir.path() + "/Song.m3u8");
    QCOMPARE(song.value("isDir").toBool(), false);
}

#include "tst_picker_support.moc"
QTEST_MAIN(TestPickerSupport)
