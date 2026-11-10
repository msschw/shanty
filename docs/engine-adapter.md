# SH-402 — Audio engine adapter: backend, host/device split, device checklist

*(Phase B documentation for ticket SH-402, Epic SH-E4. Code:
`adapters/engine/AudioEngineAdapter.{h,cpp}`; port:
`core/ports/IAudioEngine.h`; host suite: `tests/engine_adapter/`.)*

## Backend verdict (recorded decision SH-D402-1)

**Shanty's engine adapter rides QtMultimedia (`QMediaPlayer`).** Sailfish OS
ships its own Qt Multimedia module — the Jolla/Mer fork
[`sailfishos/qtmultimedia`](https://github.com/sailfishos/qtmultimedia)
(branch `mer-5.6`, the Qt 5.6 era still used by Sailfish `qt5-multimedia`
packages). That fork builds two platform backends that matter for a local
music player:

- a **GStreamer multimedia backend** that implements `QMediaPlayer` —
  decoding (MP3 / OGG Vorbis / FLAC) happens in GStreamer; on native hardware
  adaptations decoding is bridged through `gst-droid`/Android HAL plugins,
  on ordinary builds (incl. the x86 emulator) through the standard
  `gst-plugins-{base,good,ugly}` software decoders;
- a **PulseAudio audio backend** (`pulseaudio` plugin) that fires
  `QAudioOutput`-class output through the Sailfish `pulseaudio` stack, plus a
  `resourcepolicy` hook so media playback interacts with the audio
  resource/priority framework (`ngfd`/resource-policy) like every first-party
  media app.

This is the same stack the stock Sailfish media player and community players
(e.g. the `amber-*` / community qtmultimedia players) use, so `QMediaPlayer`
is not an exotic choice — no additional low-level code is needed. Sailfish
targets Qt 5.6-style APIs, so the adapter uses
`QMediaPlayer::setMedia()`/`mediaChanged` (5.6) rather than the 5.15+
`setSource()`/`sourceChanged` renaming.

**Never compiled on the host.** The host build here has *no* QtMultimedia
development environment, and the device backend cannot be host-compiled:
the engine adapter therefore keeps a documented stub variant on the host and
compiles the full backend only where `qtHaveModule(multimedia)` holds inside
a non-host (`!onHost`) build (see *decisions* below).

## Design decisions recorded for this ticket

| ID | Decision |
| --- | --- |
| SH-D402-1 | Backend: QtMultimedia `QMediaPlayer` (Sailfish fork, GStreamer decode → PulseAudio output + resource policy). No direct GStreamer/PulseAudio code. |
| SH-D402-2 | Host/device split: the host-buildable subset is QtCore-only (`QUrl` helpers + stub variant: construction-safe, transport no-ops, `state()==Idle`, no callbacks ever fired on host). Device code is guarded by `#ifdef HAVE_QTMULTIMEDIA`, enabled in `adapters/adapters.pro` via `!onHost { qtHaveModule(multimedia) { … } }` with both `CONFIG += HAVE_QTMULTIMEDIA` and `DEFINES += HAVE_QTMULTIMEDIA`. |
| SH-D402-3 | URL mapping (design D3): plain UTF-8 paths ⇄ `file://` via public static helpers `toLocalUrl()` / `toPlainPath()` — `QUrl::fromLocalFile`/`toLocalFile` semantics. Empty path → empty `QUrl`; non-local/empty `QUrl` → `""`. Path names with spaces, `%`, punctuation and non-ASCII Unicode round trip byte-exact; non-UTF-8 byte sequences are **unsupported** (D3 premise). |
| SH-D402-4 | Error mapping (device only, QMediaPlayer → `PlaybackError`): `FormatError` → `SkippedUnsupported` (SH-403 skips + advances); `ResourceError`/`AccessDeniedError` → `DecodeFailure` (fatal for the entry); `ServiceMissingError`/network/other → `EngineFailure`. |
| SH-D402-5 | Natural end-of-track: `onTrackFinished()` fires ONLY on `StoppedState + mediaStatus == EndOfMedia` — a plain `stop()`/pause/fast media switch also visits `StoppedState` and must not look like a finished track. |

Callbacks, not exceptions: every failure path flows through the
`IAudioEngine` callbacks (`onTrackFinished`, `onError`); nothing in the
adapter throws.

## Manual device checklist (emulator or device)

Run in the Sailfish SDK; each row ends with a **placeholder for the observed
result** — acceptance for SH-402 is this checklist complete. Prepare one
sample per supported format (MP3, OGG, FLAC), pushed to the device, e.g.
`/home/nemo/Music/sample.mp3`, `sample.ogg`, `sample.flac`.

1. **Push + discover**: `sfdk device push` (or scp) all three samples into
   a folder the app's library view manages; run a library scan.
   → *RESULT: __*
2. **Play one MP3 via the Player view** (transport = playback controller →
   `AudioEngineAdapter`): sound actually audible on the speaker output
   (emulator audio or device speaker/headphones), no silent run.
   → *RESULT: __* (device/emulator: __)
3. **Play one OGG the same way** → audible, no warning banner.
   → *RESULT: __*
4. **Play one FLAC the same way** → audible; note any start-up delay vs.
   MP3 (codec cold start in GStreamer).
   → *RESULT: __*
5. **Position/duration**: progress bar advances (≥ 2 position updates in
   e.g. 5 s), total duration matches the sample's real length (via
   `durationSeconds()`), position starts at 0 after `stop()` → `play()`.
   → *RESULT: __*
6. **End of track**: let a short sample run out; PlaybackController stops at
   the last queue entry (no auto-wrap, per SH-401 semantics).
   → *RESULT: __*
7. **Unsupported file probe** (SH-403 interplay): hand the engine a `.txt`
   renamed to `.mp3` → clean `SkippedUnsupported` skip + advance, no crash.
   → *RESULT: __*
8. **Headphone connector**: plug headphones mid-playback — audio reroutes
   (PulseAudio routing confirmation), no restart required by the app itself;
   pull the plug — playback continues/re-routes to speaker.
   → *RESULT: __*
9. **Permissions/Silica notes**: playback needs no extra Store-side
   permission (Harbour API checklist: `QtMultimedia` QML Multimedia is a
   *banned* QML import — our C++ `QMediaPlayer` adapter + allowed
   `qt5-qtmultimedia*` RPM dependency is the Harbour-compatible route).
   Note any `DBus` / resource-policy prompt the first time playback starts,
   and any AV "multitasking" cover-action behaviour the app must opt into.
   → *RESULT: __*
10. **Repeat a full pass after device reboot** (stateless adapter check —
    no engine-side persistence is assumed).
    → *RESULT: __*

## Caveats for later tickets (from investigation)

- **No gapless:** `QMediaPlayer` 5.6 performs `setMedia()` before `play()`,
  so a short decode-start gap between consecutive tracks is expected
  (SH-404's "gapless advance" is command-level, not sample-accurate).
- **Equalizer:** QtMultimedia on Sailfish exposes no equalizer API — the
  SH-502 effect-chain section needs its own investigation (GStreamer EQ
  elements via a custom pipeline or PulseAudio module-equalizer-sink).
- **Position granularity** is milliseconds in Qt, seconds at the port
  boundary (`positionSeconds()`), matching SH-404's passthrough contract.
