// SH-902 Phase A: profile → device-band mapping (pure logic, core).
// SPDX-License-Identifier: GPL-3.0-or-later
// Red-first: the mapping stub returns a flat 10-band vector, so the
// shape cases FAIL BY ASSERTION (never by crash). Contract under test —
// core/domain/EqualizerBandMapping.h:
//   - device grid = gst equalizer-10bands centers (31.25 Hz … 16 kHz);
//   - piecewise-linear curve in log2(f) space, sampled at the device
//     band centers (the smooth limit of frequency-overlap weighting);
//   - 0 dB outside the points' frequency span (edge folding of
//     out-of-range points happens via in-span interpolation);
//   - empty / fewer-than-2-points input → flat;
//   - identity for a profile already on the device grid;
//   - unsorted input accepted (sorted internally);
//   - outputs clamped to ±12 dB (EqualizerProfile::kMaxAbsGainDb).
#include <QtTest>

#include <cmath>
#include <utility>
#include <vector>

#include "domain/EqualizerBandMapping.h"

namespace {

using shanty::DeviceEqualizerBands;
using Points = std::vector<std::pair<double, double>>;

Points points(const double (&pairs)[2][10], int count) {
    Points result;
    for (int i = 0; i < count; ++i)
        result.emplace_back(pairs[i][0], pairs[i][1]);
    return result;
}

void verifyBands(const std::vector<double>& actual,
                 const double (&expected)[DeviceEqualizerBands::kBandCount],
                 const char* context) {
    QVERIFY2(actual.size() == DeviceEqualizerBands::kBandCount,
             qPrintable(QString("%1: result must carry exactly %2 gains, got %3")
                            .arg(context)
                            .arg(DeviceEqualizerBands::kBandCount)
                            .arg(actual.size())));
    for (int band = 0; band < DeviceEqualizerBands::kBandCount; ++band) {
        QVERIFY2(std::abs(actual[band] - expected[band]) < 1e-9,
                 qPrintable(QString("%1: band %2 (%3 Hz) expected %4 dB, got %5 dB")
                                .arg(context)
                                .arg(band)
                                .arg(DeviceEqualizerBands::kFrequenciesHz[band])
                                .arg(expected[band])
                                .arg(actual[band])));
    }
}

}  // namespace

class TestEqMapping : public QObject {
    Q_OBJECT

private slots:
    void emptyPointsYieldFlatDeviceBands();
    void gridConstantsMatchGstEqualizer10Bands();
    void singlePointCarriesNoSlopeInformation();
    void exactDeviceGridProfileIsIdentity();
    void shapedBetweenPointsInterpolatesLogSpaced();
    void midpointPointSplitsBetweenNeighbors();
    void belowDeviceRangeFoldsIntoLowestBand();
    void aboveDeviceRangeFoldsIntoHighestBand();
    void unsortedInputPointsAreSortedBeforeMapping();
    void outOfRangeGainsAreClampedToProfileDomain();
};

// (1) No points at all → every device band 0 dB (flat/bypass).
void TestEqMapping::emptyPointsYieldFlatDeviceBands() {
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(Points{});
    const double flat[DeviceEqualizerBands::kBandCount] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    verifyBands(mapped, flat, "empty input");
}

// (2) The grid constant is a platform fact: gst equalizer-10bands
// defaults (docs/eq-investigation.md). Green by design at Phase A.
void TestEqMapping::gridConstantsMatchGstEqualizer10Bands() {
    QCOMPARE(DeviceEqualizerBands::kBandCount, 10);
    const double gst[DeviceEqualizerBands::kBandCount] = {
        31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
    verifyBands(std::vector<double>(std::begin(DeviceEqualizerBands::kFrequenciesHz),
                                    std::end(DeviceEqualizerBands::kFrequenciesHz)),
                gst, "grid constant");
}

// (3) One point is a location without a slope — the curve model gives
// flat (documented decision; a one-point "profile" must not smear its
// gain across the whole spectrum).
void TestEqMapping::singlePointCarriesNoSlopeInformation() {
    const double pairs[2][10] = {{250.0, -3.0}};
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(points(pairs, 1));
    const double flat[DeviceEqualizerBands::kBandCount] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    verifyBands(mapped, flat, "single point");
}

// (4) A profile already on the device grid maps 1:1, order preserved —
// the manual-profile path (gains authored against the device bands).
void TestEqMapping::exactDeviceGridProfileIsIdentity() {
    const double gains[DeviceEqualizerBands::kBandCount] = {
        -2.0, 5.0, -7.0, 3.0, 0.0, -11.0, 8.0, 1.0, -4.0, 6.0};
    Points grid;
    for (int band = 0; band < DeviceEqualizerBands::kBandCount; ++band)
        grid.emplace_back(DeviceEqualizerBands::kFrequenciesHz[band], gains[band]);
    verifyBands(shanty::mapGainsToDeviceBands(grid), gains, "device-grid identity");
}

// (5) Points BETWEEN device centers pull the covered bands through the
// log-domain curve (frequency-overlap weighting, smooth limit):
// 62.5 Hz → 500 Hz over three equally log-spaced segments.
// Expected: 31.25 untouched (below span); 62.5=-3; 125=-5; 250=-7;
// 500=-9; 1k+ untouched (above span).
void TestEqMapping::shapedBetweenPointsInterpolatesLogSpaced() {
    const double pairs[2][10] = {{62.5, -3.0}, {500.0, -9.0}};
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(points(pairs, 2));
    const double expected[DeviceEqualizerBands::kBandCount] = {
        0.0, -3.0, -5.0, -7.0, -9.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    verifyBands(mapped, expected, "in-span interpolation");
}

// (6) Log-space midpoint: a +6 dB point at 1k and a -6 dB point at 4k
// give 0 dB at 2k (the log midpoint), full gains at the anchors, and
// nothing outside the span.
void TestEqMapping::midpointPointSplitsBetweenNeighbors() {
    const double pairs[2][10] = {{1000.0, 6.0}, {4000.0, -6.0}};
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(points(pairs, 2));
    const double expected[DeviceEqualizerBands::kBandCount] = {
        0.0, 0.0, 0.0, 0.0, 0.0, 6.0, 0.0, -6.0, 0.0, 0.0};
    verifyBands(mapped, expected, "log midpoint");
}

// (7) Out-of-range point BELOW the device range folds into the lowest
// band via the in-span curve: 20 Hz..62.5 Hz at +6 → bands 0 and 1
// follow the (flat) segment; 125 Hz and above stay flat.
void TestEqMapping::belowDeviceRangeFoldsIntoLowestBand() {
    const double pairs[2][10] = {{20.0, 6.0}, {62.5, 6.0}};
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(points(pairs, 2));
    const double expected[DeviceEqualizerBands::kBandCount] = {
        6.0, 6.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    verifyBands(mapped, expected, "low fold");
}

// (8) Same on the top edge: 8 kHz..20 kHz at +4 → bands 8 and 9 ride
// the segment; 4 kHz and below stay flat.
void TestEqMapping::aboveDeviceRangeFoldsIntoHighestBand() {
    const double pairs[2][10] = {{8000.0, 4.0}, {20000.0, 4.0}};
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(points(pairs, 2));
    const double expected[DeviceEqualizerBands::kBandCount] = {
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0, 4.0};
    verifyBands(mapped, expected, "high fold");
}

// (9) Order-independence: the AutoEq reader guarantees ascending
// frequencies, but the mapping is a pure function — it must not care.
void TestEqMapping::unsortedInputPointsAreSortedBeforeMapping() {
    const double gains[DeviceEqualizerBands::kBandCount] = {
        -2.0, 5.0, -7.0, 3.0, 0.0, -11.0, 8.0, 1.0, -4.0, 6.0};
    Points reversed;
    for (int band = DeviceEqualizerBands::kBandCount - 1; band >= 0; --band)
        reversed.emplace_back(DeviceEqualizerBands::kFrequenciesHz[band], gains[band]);
    verifyBands(shanty::mapGainsToDeviceBands(reversed), gains,
                "reversed grid input");
}

// (10) Defensive clamp: even malformed input (gains beyond the profile
// domain) cannot push the device bands outside [-12, +12] dB.
void TestEqMapping::outOfRangeGainsAreClampedToProfileDomain() {
    const double pairs[2][10] = {{31.25, 30.0}, {62.5, -30.0}};
    const std::vector<double> mapped =
        shanty::mapGainsToDeviceBands(points(pairs, 2));
    const double expected[DeviceEqualizerBands::kBandCount] = {
        12.0, -12.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    verifyBands(mapped, expected, "clamped");
}

QTEST_MAIN(TestEqMapping)
#include "tst_eq_mapping.moc"
