# Shanty — Architecture, Epics & Tickets

Derived from `PROJECT.md` (charter) and the current blank Sailfish OS
template repository. This document is the working backlog:

**Workflow for implementers:**
1. Every ticket below is split into **Phase A (tests)** and
   **Phase B (functional code)**.
2. Phase A must produce committed, failing-reason-sound tests (compiling,
   running, and failing with `unimplemented` TODOs) **before** Phase B starts.
3. No ticket's Phase B may be started by an agent before its Phase A passes
   review, and no dependent epic's tests may assume behaviour not specified
   in an earlier epic's interface.
4. Core logic lives in pure C++ without QML or Sailfish dependencies, so all
   Phase A tests run on the host (see E1). Agents implementing QML/packaging
   can work against interfaces only.

---

## 1bis. Phase A execution status

Implemented so far. Runner: `scripts/run-tests.sh` (host mode,
`CONFIG+=onHost`, build tree `build-host/`, binaries in
`build-host/tests/bin/`). **All 12 host suites are GREEN after wave 2**
(SH-102, SH-202..205, SH-304, SH-305 landed).

**Wave 1 Phase B landed:** SH-201, SH-301+302+303, SH-401, SH-501 are green.
Decision records: queue stop-at-end (SH-401, in `PlaybackQueue.h`);
M3U metadata storage is unicode-preserving — `QString::fromStdString(title)`
yields the true Unicode string — and `.m3u` export keeps real Latin-1 when
every code point fits, else emits UTF-8 bytes (interop note: export via
`.m3u8`) — SH-302/303 decisions in `M3uPlaylistCodec.cpp`; profile wire
format is a line-based `name⇥gains` record with escaped names and exact
double round trips via `max_digits10` — SH-501 decision in
`SettingsService.cpp`. (`Playlist::add` was implemented as a minimal
sanctioned exception during SH-301 so `parse()` can build a playlist.)
Suite `.pro` files now carry `PRE_TARGETDEPS` on the static libs so binaries
relink after core/adapters changes.

**Wave 2 Phase B landed:** SH-202 (LibraryService scan + filter
composition; `FilterRule`/`isUnderPath` implemented, blacklist beats
whitelist, whitelist list-entry prefix includes file stems), SH-203
(FilesystemScanner: recursive, sorted+deduped, supports-extension filtered,
non-existing roots ignored), SH-204 (FileIndexStore: file-backed config
with `SHANTY_LIBRARY_STORE` env override, else
`GenericConfigLocation/shanty/library.conf`, `tag=value` lines, atomic
`QSaveFile` writes; fresh in-process store reads empty but adopts files
written by other processes), SH-304 (Playlist mutations), SH-305
(PathResolver: `file://` + percent-decoding, malformed sequences kept
verbatim), SH-102 (GPL-3.0 LICENSE, SPDX headers, spec License tag).

**Wave 3 Phase B landed:** SH-403/404 (PlaybackController facade over
PlaybackQueue + IAudioEngine; decision matrix encoded in
`core/playback/PlaybackController.h`: EngineFailure/DecodeFailure → fatal
stop with no auto-skip, SkippedUnsupported → skip + advance, end-of-track at
the last entry → stop, no wrap-around; invalid commands on an empty queue →
documented no-ops surfaced via an `invalidCommand` callback), SH-502
(EffectChainService: band-count mismatch → reject without touching the
chain, validity checked before count, `reset()` = `bypass()` + explicit
zero gains, never drives transport), SH-503 (AutoEqReader adapter +
`IAutoEqReader` port + `AutoEqParseResult`: parses the EqualizerAPO
`GraphicEQ: f g; …` line and `frequency,gain` CSV; bands = the file's own
frequencies ascending; out-of-±12 dB gains parse fine but yield
`isValid()==false`; `ProfileSource {Manual, Imported}` added additively to
`EqualizerProfile`). All wave-3 code was folded into the shared static
libs (`core.pro`, `adapters.pro`) after the parallel agents landed.
`PathResolver.h` duplicate forward declaration cleaned.

**Wave 4 Phase B landed (logic):** SH-402 (AudioEngineAdapter:
host-buildable QtCore-only subset with construction sanity + D3 URL
mapping (`toLocalUrl`/`toPlainPath` statics); device variant
`#ifdef HAVE_QTMULTIMEDIA` over the Sailfish Qt Multimedia fork
(QMediaPlayer → GStreamer/PulseAudio, Qt 5.6-era API — no gapless, no Qt
equalizer: recorded in `docs/engine-adapter.md`, which also carries the
manual device checklist); backend investigation verdict: QMediaPlayer, the
same stack the stock Sailfish media player uses), SH-601..605 view-model
Phase A logics: NavigationViewModel (LIFO page stack over
Library/Settings/Player — pushes only from the root, sibling pushes and
root pops are documented invalid no-ops), LibraryViewModel
(reject-on-bad-add, idempotent removals, set-semantics filter lists,
immediate save per effective mutation, no filtering re-implementation),
PlaylistService + PlaylistViewModel (import applies
PathResolver::resolveEntryPaths at the service boundary — D3; export
overwrites unconditionally; mutations deliver exactly one
changed-after-mutation event), PlayerViewModel (refresh()-timed
observation, clamped seek with rejection on unknown duration, queue-edge
`canGoNext`/`canGoPrevious` under stop-at-end, fatal-code-gated
transportEnabled), SettingsViewModel (CRUD through SettingsService with
rename = save+delete composite, `equalizer/current` selection key,
per-apply EffectChainService with band-count construance, AutoEq import
stamped `ProfileSource::Imported`).

New shared component: `presentation/` (pure-C++ view-models, static lib
`shanty-viewmodels`), folded after the wave; suites link it via
`PRE_TARGETDEPS`/`LIBS` (note: link order matters — suites that use core
services through view models link `-lshanty-viewmodels` before
`-lshanty-core`).

**SDK bring-up landed (wave 6):** first successful cross-build in the
Sailfish SDK (`/home/craw/SailfishOS/bin/sfdk`, target
`SailfishOS-5.1.0.11-aarch64`) → `RPMS/shanty-0.1-1.aarch64.rpm` with the
full install set verified in the payload (`/usr/bin/shanty`,
`/usr/share/applications/shanty.desktop`, 4 hicolor icon sizes,
`/usr/share/shanty/qml` tree, `shanty-de.qm`). Fixes required along the
way, all recorded: `qEnvironmentVariable()` (Qt ≥5.10) replaced with the
`qEnvironmentVariableIsSet()/qgetenv()` pair in `FileIndexStore.cpp`
(Sailfish target ships Qt 5.6); `app/app.pro` template comment converted
from `//` to `#` (qmake only accepts `#`); template artifacts moved into
the canonical `app/` layout (`git mv` of `qml/`, `icons/`,
`shanty.desktop`) so `sailfishapp.prf`'s relative resolution works — the
prf is processed after `.pro` parsing, so late variable overrides lose to
it; `tests/packaging` path contract updated accordingly (desktop +
`app/icons/<size>` + `app/qml` scan; DISTFILES case now resolves `../`
tokens against the repo root and plain tokens against `app/`).
sfdk performs in-tree RPM builds — generated `Makefile*`/`*.o`/`*.a`/
`*.qm`/`/RPMS/` artifacts are gitignored. Note: sfdk output paths appear
target-root-prefixed (`.../mersdk/targets/<target>/usr/...`) due to
build-shell output translation; payload paths are standard `/usr/...` on
device.

| Ticket | Test suite | Status |
| --- | --- | --- |
| SH-101 | `tests/*` harness + `core/`, `adapters/`, `app/` layout — build green | infrastructure in place |
| SH-102 | `tests/license` — 2 RED (missing SPDX in `app/shanty.cpp`; spec lacks `License: GPL-3.0`) | tests done, red as required |
| SH-201 | `tests/track` — 10 cases — **GREEN** | Phase B done |
| SH-202 | `tests/library_scan` — 9 cases, 5 RED (scan + filter composition) | tests done |
| SH-203 | `tests/filesystem_scanner` — 7 cases, 3 RED (adapter on temp fixtures) | tests done |
| SH-204 | `tests/library_store` — 5 cases, 2 RED (save/load round trip) | tests done |
| SH-301 | `tests/m3u_parse` — 10 cases — **GREEN** | Phase B done |
| SH-302 | `tests/m3u_encoding` — 6 cases — **GREEN** | Phase B done |
| SH-303 | `tests/m3u_roundtrip` — 7 cases — **GREEN** | Phase B done |
| SH-304 | `tests/playlist_ops` — 9 cases — **GREEN** | Phase B done |
| SH-305 | `tests/m3u_resolve` — 7 cases — **GREEN** (relative/absolute/file:// + percent-decoding) | Phase B done |
| SH-401 | `tests/playback_queue` — 13 cases — **GREEN** | Phase B done |
| SH-501 | `tests/equalizer_profile` — 10 cases — **GREEN** | Phase B done |
| SH-402 | `tests/engine_adapter` — 10 cases — **GREEN** (host sanity subset; sound output = manual device checklist in `docs/engine-adapter.md`) | Phase B done (host part; device pass pending) |
| SH-403/404 | `tests/playback_controller` — 20 cases — **GREEN** (transport + end-of-track + error matrix) | Phase B done |
| SH-502 | `tests/effect_chain` — 12 cases — **GREEN** | Phase B done |
| SH-503 | `tests/autoeq` — 12 cases — **GREEN** (GraphicEQ + CSV subsets) | Phase B done |
| SH-601 | `tests/navigation` — 9 cases — **GREEN** (view-model logic; Silica shell = Phase B, device) | Phase A done |
| SH-602 | `tests/library_vm` — 22 cases — **GREEN** (Silica library page = Phase B, device) | Phase A done |
| SH-603 | `tests/playlist_vm` — 25 cases — **GREEN** (Silica playlist page = Phase B, device) | Phase A done |
| SH-604 | `tests/player_vm` — 20 cases — **GREEN** (Silica player page = Phase B, device) | Phase A done |
| SH-605 | `tests/settings_vm` — 21 cases — **GREEN** (Silica settings page = Phase B, device) | Phase A done |
| SH-701 | `scripts/run-tests.sh` (single entry point: tree arg, non-destructive, real exit codes) + `docs/acceptance-checklist.md` (automated gates + 22-row manual device checklist incl. PENDING rows for SH-601..605 Phase B + conclusion rubric) | done (checklist rows awaiting device) |
| SH-702 | `tests/packaging` — 8 cases — **GREEN** (spec template keys, changelog, install set vs app template, icon sizes, DISTFILES/translations catalogue with documented soft-skip for qstr parity; `rpm/shanty.spec` finalised: runtime Requires + charter description + %changelog; `docs/app-size-audit.md`: <2 MB binary ceiling, 5-dep runtime list, SDK accounting procedure) | Phase B done |

**Sweep after wave 5: all 22 host suites GREEN (`make check` exit 0,
256 Totals-passed incl. fixtures).**

**Sweep after wave 4: all 21 host suites GREEN (`make check` exit 0,
248 cases; each suite's Totals also counts its init/cleanup fixtures).**

**Sweep after wave 6: all 22 host suites GREEN (`make check` exit 0)
after the artifact move; SDK aarch64 RPM build green.**

- Remaining work: SH-E6 Phase B (Silica UI — view-model logics are green,
  SDK cross-build verified) and the manual device rows in
  docs/acceptance-checklist.md (incl. the SH-402 sound-output checklist).
- Stub policy: every unimplemented code path carries a
  `TODO(SH-xxx Phase B)` marker and returns documented "unimplemented"
  values; Phase B agents must replace them so the corresponding suite turns
  green *without modifying test expectations* (test comments document the
  decisions; truly open decisions are flagged in ISSUES.md §2).

---

## 1. Architecture

### 1.1 Layers

```
┌────────────────────────────────────────────────────────────┐
│  Presentation (QML / Sailfish Silica)                      │
│    Library view · Settings view · Player view · Cover      │
└──────────────▲─────────────────────────────────────────────┘
               │ exposed via QObject facades (controllers)
┌──────────────┴─────────────────────────────────────────────┐
│  Application services (Qt, testable, no QML)               │
│    LibraryService · PlaylistService · PlaybackController   │
│    SettingsService                                         │
└──────────────▲─────────────────────────────────────────────┘
               │ depends on port interfaces only (Dependency Inversion)
┌──────────────┴─────────────────────────────────────────────┐
│  Domain core (pure C++, no Qt/QML, fully unit-testable)    │
│    Track · Playlist · PlaylistEntry · MediaLibrary         │
│    FilterRule (whitelist/blacklist) · EqualizerProfile     │
└────────────────────────────────────────────────────────────┘
               ▲ implemented by
┌──────────────┴─────────────────────────────────────────────┐
│  Adapters (thin, framework-bound)                          │
│    M3uPlaylistCodec · FilesystemScanner · FileIndexStore   │
│    AudioEngineAdapter (Sailfish media backend)             │
│    DcpSettingsStore (dconf-backed settings)                │
└────────────────────────────────────────────────────────────┘
```

### 1.2 Ports (interfaces in the spirit of SOLID)

| Port | Purpose | Implemented by |
| --- | --- | --- |
| `IPlaylistCodec` | parse/serialize a playlist format (M3U today; extensible later — Open/Closed) | `M3uPlaylistCodec` |
| `ILibraryScanner` | enumerate audio files under managed folders | `FilesystemScanner` |
| `ILibraryStore` | persist library configuration (folders, whitelist, blacklist) | `FileIndexStore` (dconf/sandbox settings file) |
| `IAudioEngine` | play/pause/stop/seek/next/previous, position & duration, equalizer control | `AudioEngineAdapter` |
| `IEffectChain` | equalizer/AutoEq application to the audio output | `AudioEngineAdapter` effect section |
| `ISettingsStore` | read/write generic persisted settings | `DcpSettingsStore` (host test double: in-memory map) |

### 1.3 Component map (qmake subdirs)

```
shanty.pro                → subdirs template project
  core/                   → domain + services + ports (no QML, testable on host)
  adapters/               → M3U codec, filesystem scanner, settings store,
                            audio engine adapter
  app/                    → QObject controllers/facades + Sailfish main
  qml/                    → Silica UI (Library, Settings, Player, Cover)
  tests/                  → host-runnable unit tests per epic
```

Test framework: **Qt Test** (`QTest`), because it is already available in the
Sailfish SDK and works on the host with plain qmake. Fake/stub implementations
of every port live in `tests/fakes/`.

### 1.4 Key design decisions

- **D1:** The domain (Track, Playlist, filters, codec results) is POD/value
  types in plain C++; Qt usage starts at the service layer.
- **D2:** Playback algorithms (queue ordering, next/previous) are pure logic
  owned by `PlaybackQueue` in core; the engine adapter only receives
  "play this URL now" commands. This keeps transport logic testable.
- **D3:** File path handling: the domain only deals with plain paths in UTF-8;
  all `QUrl`/`file://` conversion happens in the adapters/application layer.
- **D4:** M3U variants (`m3u` = bytes/Latin-1 extended fallback, `m3u8` =
  UTF-8) are handled inside `M3uPlaylistCodec`; the domain sees only parsed
  entries.
- **D5:** AutoEq profiles are treated as data (`EqualizerProfile`) loaded
  through an `AutomationProfile` abstraction later (E5), so profile *sources*
  (imported preset files vs. manually tuned) can vary without touching the UI.

---

## 2. Epics & Tickets

Ticket IDs may be used as branch/commit references
(`epic/SH-E3-tickets`, `SH-302-phase-a` etc.).

---

### Epic SH-E1 — Build & Test Infrastructure *(enables all TDD work)*

> Goal: restructure the blank template into the component layout of §1.3 and
> get host-runnable unit tests green before any feature work. **Everything
> else depends on this.**

#### SH-101 Host-testing build scaffold

- **Phase A (tests):** Create `tests/` with a trivial self-test that
  verifies the harness works on the host (` QVERIFY(true) is not enough —
  assert the build links `core` and can instantiate one domain type).
  Acceptance: `qmake && make && make check` (or `ctest`) runs on the host SDK
  and on plain host Qt.
- **Phase B (code):** Convert `shanty.pro` to a `subdirs` project per §1.3;
  move `src/shanty.cpp` to `app/`; add `.pri`/`.pro` fragments for `core`
  (static config: no QML imports, no Sailfish libs); wire `make check` into
  the RPM packaging build so tests gate the build.
- **Depends on:** — (start here)
- **Estimate:** S

#### SH-102 License & packaging hygiene

- **Phase A (tests):** Add a small lint test that fails if a source file has
  no GPL-3.0+SPDX header, or if `rpm/shanty.spec` lacks
  `License: GPL-3.0`.
- **Phase B (code):** Add `LICENSE` (GPL-3.0 text), SPDX headers to all files,
  fix `rpm/shanty.spec` license tag, and align the `.desktop` name/summary
  with the final app metadata.
- **Depends on:** SH-101
- **Estimate:** S

---

### Epic SH-E2 — Domain Model & Library Management

> Goal: manage folders, scan audio files (mp3/ogg/flac), apply
> whitelist/blacklist, persist library configuration.

#### SH-201 Domain value types: `Track`, `PlaylistEntry`, `FilterRule`

- **Phase A:** Tests for: construction/validation (existing file, extension
  classification of `mp3|ogg|flac`, title fallback from filename);
  `FilterRule` matching (whitelist beats blacklist; no-listed-folder default
  include; case-insensitive extensions).
- **Phase B:** Implement the types in `core/domain/`.
- **Depends on:** SH-101
- **Estimate:** S

#### SH-202 Library scan service

- **Phase A:** Tests for `LibraryService::scan()` using a fake
  `ILibraryScanner`: recursive enumeration respects managed folders;
  blacklist/whitelist filtering composes correctly; unrelated extensions
  are ignored (this also locks in the MP3/OGG/FLAC contract).
- **Phase B:** Implement the service (pure domain logic; scanner is injected).
- **Depends on:** SH-201
- **Estimate:** M

#### SH-203 Filesystem scanner adapter

- **Phase A:** Tests (host-only, using a temp dir fixture) asserting the
  adapter returns paths filtered to supported extensions, is deterministic
  (sorted), and skips unreadable directories without crashing.
- **Phase B:** Implement `FilesystemScanner` adapter against
  `QDirIterator`.
- **Depends on:** SH-202
- **Estimate:** S

#### SH-204 Library configuration persistence

- **Phase A:** Tests asserting this sequence holds: configure folders +
  whitelist/blacklist → save → reload → identical state round-trip exists.
- **Phase B:** Implement the `FileIndexStore` adapter (Sailfish sandbox
  settings file will be chosen in implementation; the port stays
  format-agnostic).
- **Depends on:** SH-201, SH-101
- **Estimate:** M

---

### Epic SH-E3 — M3U Playlists (import, edit, export)

> Goal: full parse → model → serialize round trip for `.m3u` / `.m3u8`.

#### SH-301 M3U codec: basic parsing

- **Phase A:** Tests for: `#EXTM3U` header presence/absence tolerance;
  comment lines (`#`) skipped except metadata; `#EXTINF` vs. `EXTM3U-TITLE`
  (VLC extension) field extraction; blank lines; CRLF/LF; duplicate entries;
  empty playlist; malformed playlist → structured error result (no
  exceptions as control flow on the happy path).
- **Phase B:** Implement `M3uPlaylistCodec::parse()`.
- **Depends on:** SH-101
- **Estimate:** M

#### SH-302 M3U codec: encoding rules (m3u vs m3u8)

- **Phase A:** Tests: `.m3u8` must decode as UTF-8; `.m3u` without BOM
  falls back to Latin-1 for `#EXTINF`-decoded metadata; UTF-8 BOM in unknown
  extension is sniffed; a matrix combining (path extension × payload
  encoding) is iterated in the test.
- **Phase B:** Implement encoding detection and strict/lenient paths.
- **Depends on:** SH-301
- **Estimate:** M

#### SH-303 M3U codec: serialization & round-trip

- **Phase A:** Tests: serialization emits `#EXTM3U`, `#EXTINF` lines with
  duration/title when known, URL/path lines unchanged; a parse→serialize
  round trip preserves entries and metadata (co-summarized as a requirement:
  no test asserts exact byte equality of whitespace, only semantic equality);
  export writes UTF-8 (`m3u8`) except when `.m3u` explicitly requested with
  metadata that requires Latin-1 (documented choice in ticket comments).
- **Phase B:** Implement `serialize()` + wire import/export in
  `PlaylistService`.
- **Depends on:** SH-301, SH-302
- **Estimate:** M

#### SH-304 Playlist domain: reorder/add/remove/dedup

- **Phase A:** Tests for `Playlist` operations: move entry (reorder),
  insert, remove, clear, dedupe (same path), count/total duration, corner
  cases (move on empty, remove last).
- **Phase B:** Implement `Playlist`/`PlaylistService` mutation methods.
- **Depends on:** SH-201
- **Estimate:** S

#### SH-305 Relative paths & file:// scheme resolution

- **Phase A:** Tests: entries relative to the playlist's location resolve
  against that directory when imported; absolute paths/cleaned URLs pass
  through; resolution is applied at import, persists as absolute (or
  playlist-relative per user choice — flag decided in ticket).
- **Phase B:** Implement in the codec/service boundary (D3: domain receives
  resolved plain paths only).
- **Depends on:** SH-301
- **Estimate:** S

---

### Epic SH-E4 — Playback Engine

> Goal: play local MP3/OGG/FLAC with transport controls and a queue,
> behind the `IAudioEngine` port so the Sailfish backend is swappable.

#### SH-401 Playback queue logic

- **Phase A:** Tests for `PlaybackQueue`: next/previous at edges
  (clamp or stop — decided in ticket), jump to index, live reordering
  while playing, tracks removed mid-playback, queue from playlist vs.
  from library selection.
- **Phase B:** Implement `PlaybackQueue` in core (D2).
- **Depends on:** SH-101
- **Estimate:** M

#### SH-402 Audio engine port + Sailfish adapter

- **Phase A:** Tests against a fake `IAudioEngine` driving
  `PlaybackController` (play/pause/stop, position signal throttle,
  end-of-track auto-next). Then: host tests of the adapter are limited to
  construction/URL-mapping sanity (`QUrl` ↔ plain path, D3);
  device-specific sound output is validated manually (steps listed in the
  ticket, e.g. one MP3/OGG/FLAC sample each on the emulator or device).
- **Phase B:** Implement `AudioEngineAdapter`; document the actual Sailfish
  media backend chosen and its gapless/format caveats in `docs/`.
- **Depends on:** SH-401, SH-101
- **Estimate:** L

#### SH-403 Supported-format gate

- **Phase A:** Tests: the engine's codec capability query reports
  mp3/ogg/flac as supported; unsupported (e.g. video, wav) files produce a
  controlled skip + error entry in the queue rather than a crash.
- **Phase B:** Implement capability handling; rejects go through a
  `PlaybackError` domain type.
- **Depends on:** SH-402
- **Estimate:** S

#### SH-404 PlaybackController facade (application layer)

- **Phase A:** Tests: facade exposes queue + engine + error states as
  observable, state transitions enumerated (idle→playing→paused→stopped),
  invalid commands are no-ops with a signal, behave per ticket documented
  matrix.
- **Phase B:** Implement the QObject facade used by QML.
- **Depends on:** SH-401, SH-402
- **Estimate:** M

---

### Epic SH-E5 — Audio Processing (Equalizer & AutoEq)

> Goal: player settings incl. equalizer and AutoEq profile support,
> persisted via `ISettingsStore`.

#### SH-501 `EqualizerProfile` domain + settings service

- **Phase A:** Tests: band gain validation (e.g. −12…+12 dB, configurable
  band count), default/flat profile, named-profile store/rename/delete
  round-trip through a fake `ISettingsStore`.
- **Phase B:** Implement domain type and `SettingsService`.
- **Depends on:** SH-101, SH-204 (settings store pattern)
- **Estimate:** M

#### SH-502 Effect chain port + engine wiring

- **Phase A:** Tests: applying a profile issues the expected band gains to
  a fake `IEffectChain` in defined order; bypass request resets to flat;
  profile switch mid-playback does not reset the position (asserted on
  fake engine).
- **Phase B:** Implement the adapter section in `AudioEngineAdapter`;
  document engine limitations (per-device equalizer support may vary on
  Sailfish — ticket records findings).
- **Depends on:** SH-402, SH-501
- **Estimate:** M

#### SH-503 AutoEq profile import

- **Phase A:** Tests: a representative AutoEq profile file is parsed into a
  `EqualizerProfile` (numeric tolerance specified in the ticket), invalid
  files → clean error, profile source tracked (imported vs. manual).
- **Phase B:** Implement reader (format decided after checking AutoEq's
  documented CSV/graphic-EQ output; recorded in ticket).
- **Depends on:** SH-501
- **Estimate:** M

---

### Epic SH-E6 — Presentation (Silica/QML views)

> Goal: the three dedicated views of the charter, consuming only the
> application-layer facades. UI work depends on underlying epics; visual
> behaviour beyond view-model interaction is tested manually per ticket.

#### SH-601 Application shell & navigation

- **Phase A (logic tests where applicable):** view-model state for
  navigation is a simple state machine — tests assert valid/invalid
  transitions between Library/Settings/Player.
- **Phase B (code):** Replace template pages; implement the Silica app
  shell, page stack, cover page (basic now-playing info).
- **Depends on:** SH-404, SH-501
- **Estimate:** S

#### SH-602 Library view

- **Phase A:** Tests: view model lists filtered library content, selection
  maps to playlist queue building, add/remove-folder and whitelist/blacklist
  actions delegate to `SettingsService`/`LibraryService` calls
  (asserted on fakes).
- **Phase B (code):** Folder picker (Silica `FolderPickerPage`),
  folder list management, filtered file list, multi-select actions.
- **Depends on:** SH-203, SH-204, SH-601
- **Estimate:** L

#### SH-603 Playlist view & import/export flows

- **Phase A:** Tests: view model wraps `Playlist` mutations SH-304;
  import/export dialog flow delegates to `PlaylistService` (fake codec).
- **Phase B (code):** Playlist page: entry list with drag-reorder,
  add/remove, save/export via `ContentPeerPicker`-style Sailfish flows where
  appropriate; file dialogs grown from Silica components.
- **Depends on:** SH-303, SH-304, SH-305, SH-601
- **Estimate:** L

#### SH-604 Player view & transport controls

- **Phase A:** Tests: buttons invoke `PlaybackController` with correct
  commands (play/pause toggling, next/previous); progress bar reflects
  position/duration on fake engine; disabled states at queue edges —
  decided per ticket.
- **Phase B (code):** Now-playing page with metadata, play/pause, next,
  previous, seek bar; cover shows current track.
- **Depends on:** SH-404, SH-601
- **Estimate:** M

#### SH-605 Settings view (player/EQ/AutoEq)

- **Phase A:** Tests: settings view model reads/writes through
  `SettingsService`; applying an equalizer profile assembles the expected
  call for SH-502; changes survive reload (fake store).
- **Phase B (code):** Settings page: general player options, equalizer
  bands UI, AutoEq profile list with import action.
- **Depends on:** SH-501, SH-503, SH-601
- **Estimate:** M

---

### Epic SH-E7 — Acceptance, Packaging & Release

> Goal: shippable RPM, charter-compliant acceptance runs, and lean size
> accounting.

#### SH-701 Test suite hardening & acceptance script

- **Phase A/B:** Consolidate all unit tests; add an acceptance checklist
  script in `docs/` (manual device tests: full decode of the three formats,
  import → play → edit → export round trip, library re-scan, settings
  persistence across app restart). Acceptance = checklist complete.
- **Depends on:** all epics
- **Estimate:** M

#### SH-702 RPM packaging & release

- **Phase A (tests):** Reuse SH-102's spec lint test; add assertion that
  RPM-embedded translations match the shipped QML strings.
- **Phase B (code):** Finalise `rpm/shanty.spec`, changelog, icons in all
  sizes, app size audit chapter ("lean" budget documented, e.g. binary size
  ceiling and dependency list minimal-from-charter).
- **Depends on:** SH-701
- **Estimate:** S

---

## 3. Suggested execution order for parallel agents

```
SH-101 ─┬─> SH-102            (infra)
        ├─> SH-201 ─┬─> SH-202 ─> SH-203   (library)
        │           ├─> SH-304 ──┐         (playlist ops)
        └─> SH-301 ─┬─> SH-302 ─┼─> SH-303 (m3u codec)
                    └─> SH-305 ──┘
SH-204 (persistence, after SH-101)
SH-401 ──> SH-402 ─┬─> SH-403        (playback)
                   └─> SH-404 ──> presentation epics SH-E6
SH-501 ──> SH-502/SH-503            (audio processing)
```

- Waves of parallelizable Phase-A test work: `{SH-201, SH-301, SH-401,
  SH-501}` can start as soon as SH-101 lands.
- Agents must **not** modify port interfaces owned by another agent — new
  needs go through a comment on the epic, applied by the epic owner.
