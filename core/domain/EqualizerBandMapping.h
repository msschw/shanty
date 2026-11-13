// SH-902 (SH-E9): profile → device-band mapping for the GStreamer engine.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <utility>
#include <vector>

#include "EqualizerProfile.h"

namespace shanty {

// The DEVICE equalizer is gst's `equalizer-10bands` (GstIirEqualizer10Bands,
// shipped in gstreamer1.0-plugins-good — verified on the SFOS 5.1 stack,
// docs/eq-investigation.md). Its band centers are fixed:
//   31.25 / 62.5 / 125 / 250 / 500 / 1k / 2k / 4k / 8k / 16k Hz,
// gains in dB, live-tunable via the element's band<N> properties.
struct DeviceEqualizerBands {
    static const int kBandCount = 10;
    /// Band-center frequencies (Hz), band order 0..9. Matches the gst
    /// element's default band centers.
    static const double kFrequenciesHz[kBandCount];
};

/// Maps an arbitrary N-band gain curve onto the fixed 10 device bands.
///
/// Input: (frequencyHz, gainDb) pairs in ANY order (sorted internally;
/// duplicate frequencies keep the LAST value — defensive; the AutoEq
/// reader rejects duplicates anyway). Return: exactly kBandCount gains
/// in device-band order, never empty.
///
/// Curve model (the SH-902 decision, tested in tests/eq_mapping):
///   - The profile's (frequency, gain) points form a piecewise-linear
///     curve in log2(frequency) space; each device band gets the curve
///     value at its center frequency. Log-domain interpolation is the
///     smooth limit of "frequency-overlap weighting" — it is what AutoEq
///     itself does when converting between grids — and it degrades to an
///     EXACT identity for a profile already on the device grid.
///   - Outside the points' frequency span the curve is 0 dB (a profile
///     that says nothing about a range does not touch it; symmetric with
///     the empty-profile → flat rule). Points beyond the device range
///     therefore fold into the edge bands via the in-span interpolation.
///   - Fewer than 2 points carry no slope information → flat.
///   - All outputs are clamped to EqualizerProfile's gain domain,
///     [-kMaxAbsGainDb, +kMaxAbsGainDb] (±12 dB) — profiles from
///     make()/AutoEq are already valid, so this is a defensive bound.
///
/// This mapping REPLACES the SH-502 band-count-mismatch rejection at the
/// ADAPTER boundary: EffectChainService keeps its chain-band-count
/// constrance for the chain abstraction, but when the device application
/// path (gst equalizer-10bands) is the target, an N-band profile is
/// MAPPED (this function) instead of rejected. See
/// docs/eq-investigation.md, "Fixed device band count".
std::vector<double> mapGainsToDeviceBands(
    const std::vector<std::pair<double, double>>& frequencyGainPointsHzDb);

}  // namespace shanty
