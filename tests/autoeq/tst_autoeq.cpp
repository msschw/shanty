// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later

// SH-503 Phase A: AutoEq profile import — port IAutoEqReader + adapter
// AutoEqReader (§1.1 port+adapters choice; architectural decision D5:
// AutoEq profiles are data behind an abstraction).
//
// Documented contracts for this suite (decision records also live in
// adapters/autoeq/AutoEqReader.cpp):
//
// - PARSED-IN FORMAT (subset): AutoEq's graphic-EQ export in the two
//   spellings documented in AutoEqReader.cpp (investigated 2026-11-10
//   against https://github.com/jaakkopasanen/AutoEq, see the record):
//   (a) EqualizerAPO style: one line "GraphicEQ: freq gain; freq gain; ..."
//   (b) "frequency,gain" CSV lines with frequency ascending; an optional
//       "Preamp: <x> dB" line is tolerated and recorded, not applied.
//
// - NUMERIC TOLERANCE (explicit): the reader converts decimal text to
//   double with strtod (default process locale, decimal point "."); test
//   comparisons use an absolute epsilon of 1e-9. The reader does NOT
//   round, rescale or clamp gains — AutoEq's published values are kept
//   as-is (rounding happens at serialization fin AutoEq itself, ~2
//   decimals — far inside our epsilon).
//
// - FREQUENCY→BAND MAPPING: bands are the file's own frequencies in
//   ascending order; gains() are the dB gains in that same order. The
//   reader does NOT resample into a fixed band grid (Shanty's device EQ
//   band layout) — that resampler is out-of-scope, follow-up for a
//   future ticket.
//
// - OUT-OF-RANGE GAINS: files whose gains exceed ±12 dB are still parsed
//   (raw dB values kept); the profile reports isValid() == false and the
//   SH-501 save path (SettingsService::saveProfile, see
//   tests/equalizer_profile saveTooLoudProfileIsRejected) refuses to
//   persist it.
//
// - SOURCE TRACKING: the domain default is Manual (SH-501 factories);
//   reader-produced profiles are marked Imported (SH-503 additive
//   EqualizerProfile field, no existing signature changed).

#include <QtTest>

#include <cmath>
#include <string>

#include "adapters/autoeq/AutoEqReader.h"
#include "domain/AutoEqParseResult.h"
#include "domain/EqualizerProfile.h"

using shanty::AutoEqError;
using shanty::AutoEqParseResult;
using shanty::AutoEqReader;
using shanty::EqualizerProfile;
using shanty::ProfileSource;

namespace {

// Documented numeric tolerance: absolute epsilon for parsed dB values.
constexpr double kTolerance = 1e-9;

bool fuzzyEquals(double a, double b) {
    return std::fabs(a - b) < kTolerance;
}

}  // namespace

class TestAutoEq : public QObject {
    Q_OBJECT

private slots:
    void graphicEqOneLineParsesToProfile();
    void csvPairsParseInAscendingOrder();
    void preampLineIsRecordedAndIgnoredForGains();
    void blankLinesWhitespaceUtf8BomAndCrlfTolerated();
    void emptyAndWhitespaceOnlyInputIsError();
    void nonNumericFieldErrorCarriesLineNumber();
    void gainsBeyondRangeAreParsedButInvalid();
    void nonAscendingFrequenciesAreAnError();
    void unrecognizedContentIsError();
    void sourceTrackingManualDefaultVsImported();
};

// (a) EqualizerAPO style: the full canonical AutoEq graphic-EQ line.
void TestAutoEq::graphicEqOneLineParsesToProfile() {
    AutoEqReader reader;
    // Real-world shape (trimmed to 3 pairs; real files carry ~127 log-spaced
    // pairs — see the investigation record in AutoEqReader.cpp).
    const std::string data = "GraphicEQ: 20 -0.2; 21 1.5; 20000 -3.2";

    const AutoEqParseResult result =
        reader.read(data, "autoeq/Example GraphicEQ.txt");

    QVERIFY(result.ok());
    QCOMPARE(int(result.errors.size()), 0);
    QVERIFY(result.profile.source() == ProfileSource::Imported);
    QVERIFY(result.profile.isValid());
    // Bands = the file's own frequencies, ascending (3 pairs here).
    QCOMPARE(int(result.profile.gains().size()), 3);

    const std::vector<double>& gains = result.profile.gains();
    QVERIFY(fuzzyEquals(gains[0], -0.2));
    QVERIFY(fuzzyEquals(gains[1], 1.5));
    QVERIFY(fuzzyEquals(gains[2], -3.2));

    // Name decision: the reader leaves the name empty; the naming step
    // (e.g. from the file name) is the application's job at import time.
    QVERIFY(result.profile.name().empty());
}

// (b) "frequency,gain" CSV spelling, ascending frequencies.
void TestAutoEq::csvPairsParseInAscendingOrder() {
    AutoEqReader reader;
    // Ticket example shape: ascending "20,6.0"-style pairs. The
    // "frequency,gain" header line is tolerated (skipped) when it is the
    // first non-numeric two-column line.
    const std::string data =
        "frequency,gain\n"
        "20,1.5\n"
        "1000,0.0\n"
        "20000,-3.2";

    const AutoEqParseResult result =
        reader.read(data, "import/example.csv");

    QVERIFY(result.ok());
    QVERIFY(result.profile.source() == ProfileSource::Imported);
    QVERIFY(result.profile.isValid());
    QVERIFY(!result.hasPreamp);

    const std::vector<double>& gains = result.profile.gains();
    QVERIFY(fuzzyEquals(gains[0], 1.5));
    QVERIFY(fuzzyEquals(gains[1], 0.0));
    QVERIFY(fuzzyEquals(gains[2], -3.2));
}

void TestAutoEq::preampLineIsRecordedAndIgnoredForGains() {
    AutoEqReader reader;
    // Finding: AutoEq fixed-band exports start with "Preamp: -7.5 dB".
    // EqualizerProfile has no preamp field, so the reader records it in
    // AutoEqParseResult (informational follow-up) and parses the gains
    // unchanged.
    const std::string data =
        "Preamp: -7.5 dB\n"
        "frequency,gain\n"
        "100,0.5\n"
        "500,-1.2\n";

    const AutoEqParseResult result = reader.read(data, "import/preamped.csv");

    QVERIFY(result.ok());
    QVERIFY(result.hasPreamp);
    QVERIFY(fuzzyEquals(result.preampDb, -7.5));
    QCOMPARE(int(result.profile.gains().size()), 2);
    QVERIFY(fuzzyEquals(result.profile.gains()[0], 0.5));
    QVERIFY(fuzzyEquals(result.profile.gains()[1], -1.2));
    QVERIFY(result.profile.isValid());
}

void TestAutoEq::blankLinesWhitespaceUtf8BomAndCrlfTolerated() {
    AutoEqReader reader;
    // Tolerance contract: UTF-8 BOM at the very start, CRLF and LF line
    // endings, blank/whitespace-only lines, whitespace around fields.
    const std::string data =
        "\xEF\xBB\xBF"
        "\r\n"
        "   \n"
        " frequency , gain \r\n"
        " 20 ,   1.0 \r\n"
        "\r\n"
        " 100 , -2.0 \r\n"
        "\n";

    const AutoEqParseResult result = reader.read(data, "import/spaced.csv");

    QVERIFY(result.ok());
    QCOMPARE(int(result.profile.gains().size()), 2);
    QVERIFY(fuzzyEquals(result.profile.gains()[0], 1.0));
    QVERIFY(fuzzyEquals(result.profile.gains()[1], -2.0));
}

void TestAutoEq::emptyAndWhitespaceOnlyInputIsError() {
    AutoEqReader reader;

    const AutoEqParseResult emptyResult = reader.read("", "import/e.csv");
    QVERIFY(!emptyResult.ok());
    QCOMPARE(int(emptyResult.errors.size()), 1);
    QCOMPARE(int(emptyResult.errors[0].code),
             int(AutoEqError::Code::NoData));
    QVERIFY(!emptyResult.errors[0].message.empty());
    // No exceptions escaped; profile stays the invalid empty default.
    QVERIFY(!emptyResult.profile.isValid());

    // Whitespace-only content is the same documented outcome.
    const AutoEqParseResult spacesResult =
        reader.read("   \n  \r\n\t\n", "import/s.csv");
    QVERIFY(!spacesResult.ok());
    QCOMPARE(int(spacesResult.errors[0].code),
             int(AutoEqError::Code::NoData));
}

void TestAutoEq::nonNumericFieldErrorCarriesLineNumber() {
    AutoEqReader reader;
    // Line 2 is a valid pair; line 3 has a non-numeric gain → structured
    // error value with the 1-based line number (line 0 = unknown). No
    // exceptions, no partial profile.
    const std::string data =
        "frequency,gain\n"
        "20,1.5\n"
        "1000,abc\n";

    const AutoEqParseResult result = reader.read(data, "import/bad.csv");

    QVERIFY(!result.ok());
    QCOMPARE(int(result.errors.size()), 1);
    QCOMPARE(int(result.errors[0].code), int(AutoEqError::Code::BadField));
    QCOMPARE(result.errors[0].line, 3);
    QVERIFY(!result.errors[0].message.empty());
    QVERIFY(!result.profile.isValid());
}

void TestAutoEq::gainsBeyondRangeAreParsedButInvalid() {
    AutoEqReader reader;
    // Real AutoEq normalized files carry gains down to about -15.7 dB
    // (finding: "Sennheiser HD 800 GraphicEQ.txt" ends at "19871 -15.7"),
    // beyond EqualizerProfile's ±12 dB range (kMaxAbsGainDb).
    // CONTRACT: the reader parses the raw dB values (no clamping, no
    // rescaling) and the profile is simply invalid — downstream, the
    // SH-501 save path (SettingsService::saveProfile; tested in
    // tests/equalizer_profile) rejects it, and a future ticket decides
    // about normalization when applying it to the effect chain.
    const std::string data =
        "frequency,gain\n"
        "20,13.5\n"
        "1000,-12.0\n";

    const AutoEqParseResult result = reader.read(data, "import/loud.csv");

    QVERIFY(result.ok());
    QVERIFY(result.profile.source() == ProfileSource::Imported);
    QCOMPARE(int(result.profile.gains().size()), 2);
    QVERIFY(fuzzyEquals(result.profile.gains()[0], 13.5));
    QVERIFY(fuzzyEquals(result.profile.gains()[1], -12.0));
    QVERIFY(!result.profile.isValid());  // explicitly NOT clamped to +12
}

void TestAutoEq::nonAscendingFrequenciesAreAnError() {
    AutoEqReader reader;
    // Documented subset requires strictly ascending frequencies (finding:
    // real AutoEq output is ascending log-spaced). Descending order breaks
    // the band-mapping (file order == gains order), so it is a structured
    // error, not an implicit sort.
    const std::string descending =
        "frequency,gain\n"
        "1000,1.0\n"
        "200,2.0\n";
    const AutoEqParseResult descResult = reader.read(descending, "import/d.csv");
    QVERIFY(!descResult.ok());
    QCOMPARE(int(descResult.errors.size()), 1);
    QCOMPARE(int(descResult.errors[0].code),
             int(AutoEqError::Code::FrequencyNotAscending));
    QCOMPARE(descResult.errors[0].line, 3);

    // Duplicate frequencies are equally non-ascending.
    const std::string duplicate =
        "frequency,gain\n"
        "20,1.0\n"
        "20,1.0\n";
    const AutoEqParseResult dupResult = reader.read(duplicate, "import/dup.csv");
    QVERIFY(!dupResult.ok());
    QCOMPARE(int(dupResult.errors[0].code),
             int(AutoEqError::Code::FrequencyNotAscending));
}

void TestAutoEq::unrecognizedContentIsError() {
    AutoEqReader reader;

    const AutoEqParseResult prose = reader.read("hello world\n", "import/x.txt");
    QVERIFY(!prose.ok());
    QCOMPARE(int(prose.errors.size()), 1);
    QCOMPARE(int(prose.errors[0].code),
             int(AutoEqError::Code::UnrecognizedFormat));

    // Investigated-but-out-of-subset finding: AutoEq also emits
    // FixedBandEQ.txt / ParametricEQ.txt ("Filter 1: ON PK Fc ..." lines).
    // These are not part of the SH-503 chosen subset and must yield a
    // clean error (extension goes through the port later, Open/Closed).
    const AutoEqParseResult filters = reader.read(
        "Filter 1: ON PK Fc 31 Hz Gain 6.6 dB Q 1.41\n", "import/f.txt");
    QVERIFY(!filters.ok());
    QCOMPARE(int(filters.errors[0].code),
             int(AutoEqError::Code::UnrecognizedFormat));
}

void TestAutoEq::sourceTrackingManualDefaultVsImported() {
    // Domain defaults (SH-501 factories untouched): Manual.
    QCOMPARE(int(EqualizerProfile::flat("f", 4).source()),
             int(ProfileSource::Manual));
    QCOMPARE(int(EqualizerProfile::make("m", {1.0, -1.0}).source()),
             int(ProfileSource::Manual));

    // Reader-produced profiles carry Imported (red in Phase A: the stub
    // reader errors out, so these assertions cannot even run to green).
    AutoEqReader reader;
    const AutoEqParseResult result = reader.read("GraphicEQ: 20 0.5",
                                                 "import/src.csv");
    QVERIFY(result.ok());
    QCOMPARE(int(result.profile.source()), int(ProfileSource::Imported));
}

QTEST_MAIN(TestAutoEq)
#include "tst_autoeq.moc"
