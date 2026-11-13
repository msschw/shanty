# SH-E9 investigation: the device equalizer

// Investigation record for making the SH-502 effect chain audible on the
// device. Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Status of the investigation: 2026-11-13, evidence gathered on the
// SailfishOS-5.1.0.11 emulator (the same multimedia stack a device ships).

## Background

The charter promises an equalizer with AutoEq profile support in the
Settings view. Host-side, SH-501/502/503 landed: `EqualizerProfile`
(domain), `EffectChainService` (apply semantics with validity + band-count
constrance), `AutoEqReader` (GraphicEQ + CSV parse) — all green in
`tests/equalizer_profile`, `tests/effect_chain`, `tests/autoeq`. On the
device, `AudioEngineAdapter::IEffectChain` is a documented BYPASS no-op:
the Qt 5.6 QtMultimedia fork exposes no equalizer API
(`docs/engine-adapter.md` caveat). The Settings UI therefore drives
nothing audible. This epic's job: pick a device path and make the chain
real.

## Paths investigated (evidence)

### 1. PulseAudio `module-equalizer-sink` — DEAD

- Emulator: PulseAudio **17.0** (`pulseaudio-17.0+git1-1.12.1.jolla`).
  `module-equalizer-sink` was removed in PA 16; confirmed absent from
  `/usr/lib/pulse*/modules/`.
- Verdict: not available on this platform.

### 2. PulseAudio `module-ladspa-sink` — REJECTED

- The module itself ships (`module-ladspa-sink.so`), but **no LADSPA
  plugins** are installed (`swh-plugins` / `ladspa` not packaged; no
  `/usr/lib/ladspa/`).
- Making it work would mean shipping our own LADSPA plugin `.so` and
  driving out-of-app session surgery (`load-module` per session, sink
  routing for the app's stream only). That is user-session configuration
  an app must not perform, breaks on PA session restarts, and drags a
  foreign DSP dependency into a lean app.
- Verdict: rejected (design, robustness).

### 3. Software EQ (`QAudioDecoder` → biquads → `QAudioOutput`) — FALLBACK

- Fully within the Qt 5.6 API, host-testable DSP core. But it REPLACES
  the platform decode/playback path entirely (own pipeline, own clock,
  own format negotiation, CPU cost per sample on a low-power device),
  and re-implements what the platform already does for every other
  player on the OS.
- Verdict: kept as the documented fallback if the adopted path hits a
  platform blocker. Not the leanest solution.

### 4. GStreamer `playbin` + `audio-filter` — ADOPTED (verified)

The device stack already ships the pieces; QMediaPlayer itself runs on
this same GStreamer stack:

- `gstreamer1.0 1.26.11`, `plugins-base`, `plugins-good`,
  `plugins-bad` present; **`libgstequalizer.so` lives in
  `gstreamer1.0-plugins-good`** (GstIirEqualizer family:
  `equalizer-3bands` / `equalizer-10bands`, direct-form IIR biquads,
  10 bands at 31.25 Hz … 16 kHz; presets shipped under
  `/usr/share/gstreamer-1.0/presets/`).
- `playbin` exposes the stable **`audio-filter`** property (a GstElement
  applied to the audio path) — the EQ element can be injected WITHOUT
  re-implementing decode/seek/position machinery.
- **Runtime probes (emulator, `gst-launch-1.0`, long-sample.mp3):**
  1. `playbin audio-filter=equalizer-10bands uri=…` → PREROLL → PLAYING
     through the real audio stack (`GstPulseSinkClock`, PulseAudio
     sink-input created).
  2. Same with initial gains (`band0=-6.0 band9=6.0`) → prerolls and
     plays (the element accepts gain values on construction).
- **Cross-build feasibility:** the SDK target ships the gst runtime but
  not the headers; `gstreamer1.0-devel` (gst core headers/libs — the
  whole compile surface) IS available in the configured jolla repo and
  installs into the build target via `sb2 -m sdk-install -R zypper in
  …` (Build Engine). Correction 2026-11-13: only `gstreamer1.0-devel`
  is needed — no plugins-base API is called directly, so
  plugins-base-devel is NOT installed; compile+link probe against
  `pkg-config --cflags/--libs gstreamer-1.0` verified in the target.
- Verdict: adopted. Leanest path — the platform keeps decoding, the EQ
  is one inserted element, and the IAudioEngine port (SH-402) stays the
  seam: only the engine adapter's device implementation changes.

## Consequences for the design

- **Port unchanged:** `IAudioEngine` (play/pause/resume/stop/seek +
  position/duration passthrough + callbacks) and `IEffectChain` stay as
  the seams; the QtMultimedia device variant of `AudioEngineAdapter` is
  replaced by a GStreamer one (`HAVE_GSTREAMER` replaces
  `HAVE_QTMULTIMEDIA`). The host-buildable QtCore-only sanity subset
  keeps its suite.
- **Fixed device band count:** the element is 10-band. The settings/
  profile design must decide how N-band profiles (AutoEq exports
  arbitrary frequencies; manual profiles are user-defined) map onto the
  fixed 10 bands — frequency-overlap mapping as a pure, host-tested
  function (SH-902) instead of the SH-502 band-count-mismatch rejection
  for THIS application path (the reject semantics stay in
  EffectChainService for count constrance of the chain abstraction;
  the ADAPTER mapping is a documented presentation decision). Manual
  profile editing switches to the 10 device bands (SH-605 UI follows
  the chain's band count).
- **Dependencies:** `gstreamer1.0-plugins-good` moves from "already on
  the image" to an explicit runtime `Requires` in `rpm/shanty.spec`
  (the app's core feature now contractually depends on the equalizer
  element); BuildRequires gains pkgconfig(gstreamer-1.0) — gst core
  headers/libs only (see the cross-build correction above).
- **Transport semantics to re-implement in the adapter** (playbin/bust
  driven): play/pause/resume/stop, seek (gst seek event), position
  queries (GST_QUERY), duration, track-finished (GST_MESSAGE_EOS or
  about-to-finish — the no-gapless caveat stays: playbin `about-to-finish`
  can PRE-roll the next uri, an improvement over setMedia-before-play),
  error messages (GST_MESSAGE_ERROR → PlaybackError codes per the
  SH-403 matrix mapping).

## Ticket plan

See `ISSUES_EQ.md` (untracked planning doc, same pattern as
`ISSUES_UX.md`): SH-901 tooling/spec, SH-902 band mapping (red-first
pure logic), SH-903 GStreamer engine adapter (device variant), SH-904
effect chain over the element + settings UI band-count follow-through,
device acceptance per the engine-adapter manual checklist (extended
with EQ rows).
