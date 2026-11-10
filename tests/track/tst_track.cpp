// SH-201 Phase A: Track value type + AudioFormat classification tests.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "domain/AudioFormat.h"
#include "domain/Track.h"

using shanty::AudioFormat;
using shanty::Track;

class TestTrack : public QObject {
    Q_OBJECT

private slots:
    void classifyExtensionRecognisesSupportedFormats();
    void classifyExtensionIgnoresCase();
    void classifyExtensionRejectsUnsupported();
    void isSupportedAudioPathChecksExtensionOnly();
    void makeBuildsValidTrackForSupportedFiles();
    void makeFallsBackTitleToBasenameWithoutExtension();
    void makeRejectsUnsupportedExtension();
    void makeTreatsRelativePathAsPlainPath();  // design D3
};

void TestTrack::classifyExtensionRecognisesSupportedFormats() {
    QCOMPARE(int(shanty::classifyExtension("mp3")), int(AudioFormat::MP3));
    QCOMPARE(int(shanty::classifyExtension("ogg")), int(AudioFormat::OGG));
    QCOMPARE(int(shanty::classifyExtension("flac")), int(AudioFormat::FLAC));
}

void TestTrack::classifyExtensionIgnoresCase() {
    QCOMPARE(int(shanty::classifyExtension("MP3")), int(AudioFormat::MP3));
    QCOMPARE(int(shanty::classifyExtension("Ogg")), int(AudioFormat::OGG));
    QCOMPARE(int(shanty::classifyExtension("flaC")), int(AudioFormat::FLAC));
}

void TestTrack::classifyExtensionRejectsUnsupported() {
    QCOMPARE(int(shanty::classifyExtension("wav")), int(AudioFormat::Unsupported));
    QCOMPARE(int(shanty::classifyExtension("mp4")), int(AudioFormat::Unsupported));
    QCOMPARE(int(shanty::classifyExtension("")), int(AudioFormat::Unsupported));
}

void TestTrack::isSupportedAudioPathChecksExtensionOnly() {
    QVERIFY(shanty::isSupportedAudioPath("/music/a.mp3"));
    QVERIFY(shanty::isSupportedAudioPath("/music/b.OGG"));
    QVERIFY(shanty::isSupportedAudioPath("/music/c.flac"));
    QVERIFY(!shanty::isSupportedAudioPath("/music/d.txt"));
    QVERIFY(!shanty::isSupportedAudioPath("/music/no-extension"));
    // Red herring: hidden extension after a dot in the stem
    QVERIFY(shanty::isSupportedAudioPath("/music/song.v2.mp3"));
}

void TestTrack::makeBuildsValidTrackForSupportedFiles() {
    const Track track = Track::make("/music/Artist - Song.flac");
    QVERIFY(track.isValid());
    QCOMPARE(QString::fromStdString(track.path()), QString::fromLatin1("/music/Artist - Song.flac"));
    QCOMPARE(track.format(), AudioFormat::FLAC);
    QCOMPARE(track.durationSeconds(), Track::kUnknownDuration);
}

void TestTrack::makeFallsBackTitleToBasenameWithoutExtension() {
    QCOMPARE(QString::fromStdString(Track::make("/music/Artist - Song.flac").title()), QString::fromLatin1("Artist - Song"));
    QCOMPARE(QString::fromStdString(Track::make("plain.mp3").title()), QString::fromLatin1("plain"));
}

void TestTrack::makeRejectsUnsupportedExtension() {
    QVERIFY(!Track::make("/music/notes.txt").isValid());
    QVERIFY(!Track::make("").isValid());
}

void TestTrack::makeTreatsRelativePathAsPlainPath() {
    const Track track = Track::make("music/song.mp3");
    QVERIFY(track.isValid());
    QCOMPARE(QString::fromStdString(track.path()), QString::fromLatin1("music/song.mp3"));
}

QTEST_MAIN(TestTrack)
#include "tst_track.moc"
