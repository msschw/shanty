// SH-1001 Phase A (SH-E10 UX-12): red-first tag-reader suite over the
// ITagReader port + TagLibReader adapter. Fixtures are committed tiny
// real audio files (4 s tones, tagged once with taglib itself on the
// host — generator NOT committed): tagged.mp3 (ID3v2, full set),
// tagged.ogg (Xiph, full set, non-ASCII title), partial.flac (title +
// artist only), untagged.mp3 (no tags at all).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "ports/ITagReader.h"
#include "taglib/TagLibReader.h"

using shanty::TagLibReader;
using shanty::TagMetadata;

namespace {

QString fixture(const char* name) {
    return QStringLiteral(SHANTY_ROOT) + QStringLiteral("/tests/fixtures/tags/") +
           QString::fromLatin1(name);
}

}  // namespace

class TestTagReader : public QObject {
    Q_OBJECT

private slots:
    // --- Phase B behavioural contract (red against the stub) ---

    void readReturnsFullTagsForMp3();
    void readReturnsFullTagsForOgg();
    void readReturnsNonAsciiTitleVerbatim();
    void readReturnsOnlyPresentTagsForPartialFile();
    void readReturnsEmptyTagsForUntaggedFile();
    void readReportsDurationFromAudioProperties();

    // --- Clean-miss contract (never throws) ---

    void readYieldsCleanMissForMissingFile();
    void readYieldsCleanMissForDirectoryPath();
};

void TestTagReader::readReturnsFullTagsForMp3() {
    TagLibReader reader;
    const TagMetadata md = reader.read(fixture("tagged.mp3").toStdString());
    QCOMPARE(QString::fromStdString(md.title), QStringLiteral("Amber Tone"));
    QCOMPARE(QString::fromStdString(md.artist), QStringLiteral("Shanty Ensemble"));
    QCOMPARE(QString::fromStdString(md.album), QStringLiteral("Emulator Sessions"));
    QCOMPARE(QString::fromStdString(md.genre), QStringLiteral("Electronic"));
}

void TestTagReader::readReturnsFullTagsForOgg() {
    TagLibReader reader;
    const TagMetadata md = reader.read(fixture("tagged.ogg").toStdString());
    QCOMPARE(QString::fromStdString(md.title), QStringLiteral("Café Deluxe"));
    QCOMPARE(QString::fromStdString(md.artist), QStringLiteral("Shanty Ensemble"));
    QCOMPARE(QString::fromStdString(md.album), QStringLiteral("Emulator Sessions"));
    QCOMPARE(QString::fromStdString(md.genre), QStringLiteral("Electronic"));
}

void TestTagReader::readReturnsNonAsciiTitleVerbatim() {
    TagLibReader reader;
    const TagMetadata md = reader.read(fixture("tagged.ogg").toStdString());
    // The non-ASCII title survives the round trip byte-exactly (UTF-8).
    QCOMPARE(QString::fromStdString(md.title), QString::fromUtf8("Café Deluxe"));
    QCOMPARE(md.title.size(), std::string::size_type(11));
}

void TestTagReader::readReturnsOnlyPresentTagsForPartialFile() {
    TagLibReader reader;
    const TagMetadata md = reader.read(fixture("partial.flac").toStdString());
    QCOMPARE(QString::fromStdString(md.title), QStringLiteral("Driftwood"));
    QCOMPARE(QString::fromStdString(md.artist), QStringLiteral("Shanty Ensemble"));
    // Absent tags are EMPTY STRINGS, not errors and not placeholders.
    QVERIFY(md.album.empty());
    QVERIFY(md.genre.empty());
}

void TestTagReader::readReturnsEmptyTagsForUntaggedFile() {
    TagLibReader reader;
    const TagMetadata md = reader.read(fixture("untagged.mp3").toStdString());
    QVERIFY(md.title.empty());
    QVERIFY(md.artist.empty());
    QVERIFY(md.album.empty());
    QVERIFY(md.genre.empty());
    // No tags is still a READ (not a miss): duration comes from the
    // audio properties even when no tag exists.
    QCOMPARE(md.durationSeconds, 4);
}

void TestTagReader::readReportsDurationFromAudioProperties() {
    TagLibReader reader;
    const TagMetadata mp3 = reader.read(fixture("tagged.mp3").toStdString());
    const TagMetadata ogg = reader.read(fixture("tagged.ogg").toStdString());
    const TagMetadata flac = reader.read(fixture("partial.flac").toStdString());
    QCOMPARE(mp3.durationSeconds, 4);
    QCOMPARE(ogg.durationSeconds, 4);
    QCOMPARE(flac.durationSeconds, 4);
}

void TestTagReader::readYieldsCleanMissForMissingFile() {
    TagLibReader reader;
    const TagMetadata md = reader.read("/nonexistent/shanty/missing.mp3");
    QVERIFY(md.isEmpty());  // clean miss: empty value, never a throw
}

void TestTagReader::readYieldsCleanMissForDirectoryPath() {
    TagLibReader reader;
    const TagMetadata md = reader.read(fixture("tags").toStdString());
    QVERIFY(md.isEmpty());
}

QTEST_MAIN(TestTagReader)
#include "tst_tag_reader.moc"
