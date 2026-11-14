# SH-402 — Audio engine adapter: backend, host/device split, device checklist

*(Phase B documentation for ticket SH-402, Epic SH-E4. Code:
`adapters/engine/AudioEngineAdapter.{h,cpp}`; port:
`core/ports/IAudioEngine.h`; host suite: `tests/engine_adapter/`.)*

> **SH-903 update (SH-E9, 2026-11-13): the backend decision below is
> SUPERSEDED.** The device engine now rides GStreamer directly — ONE
> `playbin` with the 10-band equalizer injected as its `audio-filter`
> (`equalizer-10bands`, ships in gstreamer1.0-plugins-good). Decision
> record: `docs/eq-investigation.md`; adapter decisions SH-D903-1..4 in
> the table below. The QtMultimedia verdict (SH-D402-1) is kept for
> history: it rode the same gst stack but exposed no equalizer API, and
> the gst core headers are now the adapter's whole compile surface
> (elements by factory name at runtime; no plugins-base API). The
> guard is `HAVE_GSTREAMER` (was `HAVE_QTMULTIMEDIA`); the host stub
> variant is unchanged in shape.

## Backend verdict (recorded decision SH-D402-1 — SUPERSEDED by SH-D903-1)

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
| SH-D806-1 | **Port unfreeze (SH-806, UX-10):** `IAudioEngine` gains `virtual void seek(long positionSeconds)` — the SH-D604-B1 deliberate no-op (the frozen port that shipped without a seek primitive) dies. Layering of the command: the VM keeps the SH-D604-B1 clamp contract (clamps into the snapshot duration; no known duration → `onSeekRejected`, no command); the controller issues `engine_.seek(t)` only from a live transport (Playing/Paused — otherwise the documented no-op `"seek: nothing playing"`); the adapter maps it 1:1 to `QMediaPlayer::setPosition(seconds * 1000)` (Qt 5.6-era milliseconds API) on the device and is a documented host stub no-op (SH-D402-2). A seek on a fresh restored session (SH-805) drops the display-only restored offset — the engine passthrough takes over. |
| SH-D903-1 | Backend (supersedes SH-D402-1): ONE gst `playbin` per adapter instance; the 10-band equalizer rides it as the `audio-filter` property (SH-E9). `gst_init(nullptr, nullptr)` in the ctor BEFORE any factory call (`gst_element_factory_make` asserts `gst_is_initialized()` — caught on the emulator's first drive). A missing playbin factory degrades to the inert variant; a missing equalizer factory degrades to playbin-without-EQ (engine works, EQ feature missing). Construction never fails. |
| SH-D903-2 | GLib main-loop integration: NO dedicated GMainLoop thread and NO g_timeout source — bus messages are POLLED on the Qt main thread by a 250 ms `QTimer` (`pollBus()`). Rationale: every gst call and every `IAudioEngine` callback then runs on ONE thread (the Qt main thread that also runs the UI) — no locks, no cross-thread callback hazards into PlaybackController. EOS latency is bounded by the poll interval. |
| SH-D903-3 | gst bus ERROR → SH-403 `PlaybackError` matrix (pure host-testable helper `mapGstError(domain, code)`): core MISSING_PLUGIN (14) and stream CODEC_NOT_FOUND (6) → `SkippedUnsupported`; resource (any) and stream (any other) → `DecodeFailure`; library and unknown domains → `EngineFailure` (never misreport a non-format failure as "unsupported"). `GError::domain` is a GQuark — matched via `g_quark_to_string()`. |
| SH-D903-4 | Pipeline lifecycle: after a gst ERROR the playbin is POISONED — a later state change fails with "state change failed and some element failed to post a proper error message" (observed on the emulator: an errored corrupt-sample play made the NEXT `play()` fail with a perfectly good file, and "Try again" kept failing; the identical playbin+filter pipeline played the same FLAC fine from `gst-launch`). gst places the reset responsibility on the application: `onGstError()` drives the playbin back to `GST_STATE_NULL`, tearing the errored decode graph down; the next `play()` re-prerolls it. |
| SH-D903-5 | qmake lazy-load quirk (device build): CONFIG-flag `.prf` files load alphabetically at the END of `.pro` parsing — once `CONFIG += link_pkgconfig` is set, `link_pkgconfig.prf` runs BEFORE `sailfishapp.prf`, so the prf's own `PKGCONFIG += sailfishapp` lands too late and `-lsailfishapp` silently vanishes from the link line. Fix: explicit `PKGCONFIG += sailfishapp` during parsing in `app/app.pro`. |

Callbacks, not exceptions: every failure path flows through the
`IAudioEngine` callbacks (`onTrackFinished`, `onError`); nothing in the
adapter throws.

## Manual device checklist (emulator or device)

Run in the Sailfish SDK; each row ends with the observed result.
Acceptance for SH-402/SH-903 is this checklist complete. Prepared
samples on the emulator (`/home/defaultuser/Music/`): `long-sample.mp3`
(60 s), `imports/tone-a.mp3` (4 s), `imports/tone-c.ogg` (4 s),
`imports/tone-d.flac` (4 s); a corrupt `bad-sample.mp3` (128 truncated
bytes) is created and removed per drive for the error row. All rows
below were driven on the emulator (SailfishOS-5.1.0.11, headless +
touchinject) against the gst backend build of 2026-11-13.

1. **Push + discover**: samples in managed folders, library scan.
   → *RESULT: OK — Tracks (52) with the corrupt probe, (51) after
   cleanup; rescan picks up/removes files correctly.*
2. **Play one MP3 via the Player view**: audibility on the headless
   emulator is verified by proxy — PulseAudio sink input present
   (`Sink Input #50`, `application.name = "shanty"`), position advances,
   launch log clean; pause corks the stream, resume uncorks.
   → *RESULT: OK (emulator drive, audibility proxy = pactl).*
3. **Play one OGG the same way** → audible, no warning banner.
   → *RESULT: OK — tone-c.ogg plays (0:03, playing), no banner.*
4. **Play one FLAC the same way** → audible; note any start-up delay vs.
   MP3 (codec cold start in GStreamer).
   → *RESULT: OK — tone-d.flac plays ("Track 2 of 2 (playing)", 0:04);
   flacdec/flacparse verified present (gst-inspect). The first attempt
   exposed SH-D903-4 (poisoned pipeline after an earlier error — NOT a
   FLAC defect; the standalone playbin probe plays the file fine).*
5. **Position/duration**: progress bar advances (≥ 2 position updates in
   e.g. 5 s), total duration matches the sample's real length (via
   `durationSeconds()`), position starts at 0 after `stop()` → `play()`.
   → *RESULT: OK — long-sample.mp3: 0:24 → 0:48 with bar fill
   following; duration bar consistent with 1:00.*
6. **End of track**: let a short sample run out; PlaybackController stops at
   the last queue entry (no auto-wrap, per SH-401 semantics).
   → *RESULT: OK — 4 s tone runs to EOS → clean stopped state; queue
   [tone-c.ogg, tone-d.flac] advances OGG → FLAC through the EOS path.*
7. **Unsupported file probe** (SH-403 interplay): hand the engine a `.txt`
   renamed to `.mp3` → clean `SkippedUnsupported` skip + advance, no crash.
   → *RESULT: OK (corrupt 128-byte "mp3" as the probe) — clean error
   banner, app alive, stopped state, no crash. The domain→code
   classification is host-unit-tested (mapGstError); this probe maps to
   DecodeFailure, and the SkippedUnsupported skip+advance path for
   MISSING_PLUGIN/CODEC_NOT_FOUND is covered by the same helper.*
8. **Headphone connector**: plug headphones mid-playback — audio reroutes
   (PulseAudio routing confirmation), no restart required by the app itself;
   pull the plug — playback continues/re-routes to speaker.
   → *RESULT: DEFERRED — no audio hardware on the headless emulator;
   hardware-only row.*
9. **Permissions/Silica notes**: playback needs no extra Store-side
   permission (gst/pulseaudio route; the QtMultimedia Harbour note below
   is obsolete — no QMultimedia import anywhere). Note any `DBus` /
   resource-policy prompt the first time playback starts, and any AV
   "multitasking" cover-action behaviour the app must opt into.
   → *RESULT: OK on the emulator — launch log clean, no prompts;
   resource-policy interplay not driven (hardware row).*
10. **Repeat a full pass after device reboot** (stateless adapter check —
    no engine-side persistence is assumed).
    → *RESULT: NOT DRIVEN this session (app relaunches verified; VM
    reboot not performed).*
11. **Seek (SH-806, UX-10)**: play a LONG sample (≥ 60 s — the 4 s tones
    are impractical: they finish during screenshot round trips); drag/tap
    the seek bar to a mid position → audible jump (content audibly changes
    position), the position label follows (`player.positionSeconds`),
    playback keeps playing (seek is not a play/pause/stop). Also verify a
    seek on a *restored* session works (paused seek → resume plays from
    the seeked position, no restart from 0).
    → *RESULT: OK — long-sample.mp3 tap-mid-bar: 0:19 → 0:48, playback
    continues; restored-session render verified at launch.*

## Caveats for later tickets (gst backend, SH-903 revision)

- **No gapless:** playbin + EOS-based advance (250 ms bus poll) — a
  short gap between consecutive tracks is possible (SH-404's "gapless
  advance" stays command-level, not sample-accurate).
- **Equalizer:** WIRED — `equalizer-10bands` is injected as the playbin
  `audio-filter` (SH-D903-1); application of profile gains is SH-904
  (SH-902 band mapping → `g_object_set`). Decision record:
  `docs/eq-investigation.md`.
- **Position granularity** is nanoseconds in gst, seconds at the port
  boundary (`positionSeconds()`), matching SH-404's passthrough contract.
- **Pipeline lifecycle (SH-D903-4):** after a gst ERROR the playbin must
  be driven to NULL by the application (the adapter does this) — reuse
  without the reset fails the next `play()`.
- **gstreamer1.0-tools** was installed on the EMULATOR during SH-903
  bring-up (gst-launch/gst-inspect probes). It is a diagnostics tool on
  the test image, NOT an app dependency — the RPM contract remains
  exactly `gstreamer1.0-plugins-good` + core (verified via
  `rpm -qp --requires`).
