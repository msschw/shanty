# Shanty — Charter acceptance checklist (SH-701)

Charter (PROJECT.md): local playback of **MP3 / OGG Vorbis / FLAC** from a
user-managed library (folders + whitelist/blacklist), **M3U import → edit →
export**, and the dedicated Library / Settings / Player views.
This document defines what "acceptance" means for SH-701 (Epic SH-E7):
**a green automated gate on the host** plus **a completed manual device test
run recorded in this file**. The engine-specific device checks of SH-402 live
in `docs/engine-adapter.md` and are cross-referenced, not duplicated.

Two surfaces exist:

| Surface | State | Covered by |
| --- | --- | --- |
| Host unit tests (core / adapters / presentation logic) | green, automated | Part A |
| Device / emulator behaviour incl. every Silica UI page | not executable yet — `app/` + Silica Phase B (SH-601..605) are pending a device session | Part B |


Part A — Automated gates (host)

Single documented entry point (SH-701):

```sh
scripts/run-tests.sh [BUILD-TREE]     # default BUILD-TREE: build-host
```

The script configures the tree with `CONFIG+=onHost` (skips `app/`), builds
core/adapters/presentation/tests, runs `make -k check` over all suite
binaries, prints a per-suite Totals table and exits non-zero if any suite
fails. It is idempotent and re-runnable — it never deletes the build tree.
Host-side equivalent commands:

```sh
mkdir -p <tree> && cd <tree> && /usr/bin/qmake ../shanty.pro "CONFIG+=onHost" && make -j4
cd tests && make -k check        # expected: exit 0
```

**Gate criteria.** Exit code 0 → every suite's line shows `failed/skipped/
blacklisted` all zero. Any failure (including the license lint gate, which
runs inside the sweep) fails the gate.

**Observed results (2026-11-10, private verification tree `build-SH-701`,
`/usr/bin/qmake`, Qt 5.15): sweep exit 0, 21 suites, 248 cases, 0 failed,
0 skipped.** Per-suite Totals, in run order (this is the authoritative
baseline; the aggregated case count in an earlier revision of ISSUES.md
§1bis, "214 cases", is stale — per-suite counts always came from these
Totals lines):

| Suite | Ticket | Cases | Failed |
| --- | --- | --- | --- |
| TestLicense | SH-102 | 4 | 0 |
| TestTrack | SH-201 | 10 | 0 |
| TestLibraryScan | SH-202 | 9 | 0 |
| TestFilesystemScanner | SH-203 | 7 | 0 |
| TestLibraryStore | SH-204 | 5 | 0 |
| TestPlaylistOps | SH-304 | 9 | 0 |
| TestM3uCodec | SH-301 | 10 | 0 |
| TestM3uEncoding | SH-302 | 6 | 0 |
| TestM3uRoundtrip | SH-303 | 7 | 0 |
| TestM3uResolve | SH-305 | 7 | 0 |
| TestPlaybackQueue | SH-401 | 13 | 0 |
| TestPlaybackController | SH-403/404 | 20 | 0 |
| TestEqualizerProfile | SH-501 | 10 | 0 |
| TestEffectChain | SH-502 | 12 | 0 |
| TestAutoEq | SH-503 | 12 | 0 |
| TestPlaylistVM | SH-603 | 25 | 0 |
| TestPlayerViewModel | SH-604 | 20 | 0 |
| TestLibraryVm | SH-602 | 22 | 0 |
| TestNavigation | SH-601 | 9 | 0 |
| TestEngineAdapter | SH-402 | 10 | 0 |
| TestSettingsVm | SH-605 | 21 | 0 |
| **Total** | | **248** | **0** |

License lint: `TestLicense` verifies (a) every `.h`/`.cpp` in the repository
carries a GPL-3.0 SPDX header within the first 10 lines and (b)
`rpm/shanty.spec` keeps `License: GPL-3.0` (the `rpm/` surface itself is
SH-702's, which concurrently adds `TestPackaging`; when that suite lands the
sweep picks it up automatically and the totals above grow accordingly — the
invariant stays "exit 0, all failure columns zero").


Part B — Manual device tests (emulator or device, Sailfish SDK)

Nothing below can be executed today: no deployable application binary
exists yet (`app/` + Silica Phase B, SH-601..605, pending their device
session). Every row therefore starts **NOT RUN**; no row may be checked
until the feature it exercises is actually deployed. Rows marked
*PENDING (SH-60x Phase B)* become executable only after that ticket's
device work lands.

Environment: Sailfish SDK, emulator or device. Prepare one audio sample per
format (MP3, OGG, FLAC) ≥ 30 s, pushed to the device e.g.
`/home/nemo/Music/`; one `.m3u8` and one `.m3u` playlist exercising relative
paths and Unicode titles (e.g. `Café Crème`, `魂/Song`) as below; one
AutoEq profile file (EqualizerAPO `GraphicEQ:` line or `frequency,gain`
CSV). Deploy with the usual `sfdk device deploy` flow once `app/` builds.

### B1 — Full decode of the three formats (engine: `docs/engine-adapter.md` rows 1–6, 9–10)

| # | Step | Expected | Observed | Status |
| --- | --- | --- | --- | --- |
| A1 | Import into library: push the three samples into a folder added in the library view, run a library scan | all three files listed (extension filter MP3/OGG/FLAC only) | __ | ☐ PENDING (needs library UI, SH-602 Phase B) |
| A2 | Play the MP3 from the player view and let it run end-to-end | audible for the whole file, plays to end and stops at queue end (no wrap-around, SH-401 semantics) | __ | ☐ PENDING (needs player UI, SH-604 Phase B) |
| A3 | Same with the OGG Vorbis sample | audible whole file, no warning/skip | __ | ☐ PENDING (SH-604 Phase B) |
| A4 | Same with the FLAC sample | audible whole file; note any cold-start delay vs MP3 (GStreamer) | __ | ☐ PENDING (SH-604 Phase B) |
| A5 | While playing each: watch position bar; after `stop()` → `play()` music restarts at 0 | position/bar advances, duration display matches the sample's real length | __ | ☐ PENDING (SH-604 Phase B) |

A2–A4 double as the engine sound-output proof of the SH-402 device
checklist (`docs/engine-adapter.md` — results may be recorded in either
document, both must stay consistent).

### B2 — M3U import → play → edit → export round trip (SH-301..305, SH-603)

| # | Step | Expected | Observed | Status |
| --- | --- | --- | --- | --- |
| B2-1 | Import `.m3u8` with relative entry paths and Unicode `#EXTINF` titles | entries resolve against the playlist's directory (SH-305), Unicode titles display correctly (UTF-8, SH-302) | __ | ☐ PENDING (needs playlist UI, SH-603 Phase B) |
| B2-2 | Import legacy `.m3u` with Latin-1 metadata and play all entries | titles correct per the documented m3u/m3u8 encoding rules; entries play in order | __ | ☐ PENDING (SH-603 Phase B) |
| B2-3 | Edit: reorder, add, remove entries, play the result | order/contents apply, playback follows the edited playlist | __ | ☐ PENDING (SH-603 Phase B) |
| B2-4 | Save the edits back to the original file; also export to a new file | exported `#EXTM3U`/`#EXTINF` file re-imports to the same entries (SH-303 round trip); Unicode titles round trip byte-correct (Latin-1 when all code points fit, otherwise UTF-8 — export via `.m3u8` for external interop) | __ | ☐ PENDING (SH-603 Phase B) |
| B2-5 | Interop check: open the exported file in another player (e.g. desktop VLC) | plays, titles acceptable | __ | ☐ PENDING (SH-603 Phase B) |

### B3 — Library re-scan and persistence (SH-202..204, SH-602)

| # | Step | Expected | Observed | Status |
| --- | --- | --- | --- | --- |
| C1 | Add a new folder with new audio files, re-scan | new files appear in the library; unsupported extensions stay out | __ | ☐ PENDING (SH-602 Phase B) |
| C2 | Blacklist a folder/file (and a whitelist folder), re-scan | blacklisted entries disappear, whitelist/blacklist composition matches the service contract | __ | ☐ PENDING (SH-602 Phase B) |
| C3 | Restart the app | managed folders + filters unchanged (persisted library configuration, FileIndexStore) | __ | ☐ PENDING (SH-602 Phase B) |

### B4 — Settings / EQ profile persistence (SH-501..503, SH-605)

| # | Step | Expected | Observed | Status |
| --- | --- | --- | --- | --- |
| D1 | Create a manual EQ profile (band gains within ±12 dB), set it as the current profile, apply | profile saved, marked current (`equalizer/current`) | __ | ☐ PENDING (needs settings UI, SH-605 Phase B) |
| D2 | Restart the app | profile still listed with its gains, current selection retained | __ | ☐ PENDING (SH-605 Phase B) |
| D3 | Import an AutoEq profile file; then a corrupt one | valid file imported with source "Imported"; corrupt file → clean error, no crash. *Audible device-side effect is out of scope here: Sailfish QtMultimedia exposes no equalizer API (caveat in `docs/engine-adapter.md`), device EQ wiring is an open SH-502 item (pipeline investigation pending)* | __ | ☐ PENDING (SH-605 Phase B) |
| D4 | Rename the current profile, restart again | rename = save + delete composite; renamed profile current, old name gone | __ | ☐ PENDING (SH-605 Phase B) |

### B5 — Silica UI surfaces (SH-601..605 Phase B device work)

| # | Surface (ticket) | Expected once implemented | Observed | Status |
| --- | --- | --- | --- | --- |
| E1 | App shell, page stack, cover page (SH-601) | Silica pages, LIFO navigation valid/invalid transitions match the NavigationViewModel contract, cover shows now-playing info | __ | ☐ PENDING (pending SH-601 Phase B device work) |
| E2 | Library page (SH-602) | folder picker, folder list management, filtered file list, multi-select actions | __ | ☐ PENDING (pending SH-602 Phase B device work) |
| E3 | Playlist page (SH-603) | entry list, drag-reorder, add/remove, save/export flows | __ | ☐ PENDING (pending SH-603 Phase B device work) |
| E4 | Player page (SH-604) | metadata, play/pause, next/previous, seek bar, cover action | __ | ☐ PENDING (pending SH-604 Phase B device work) |
| E5 | Settings page (SH-605) | general player options, EQ bands UI, AutoEq list + import action | __ | ☐ PENDING (pending SH-605 Phase B device work) |


Part C — Conclusion rubric

1. **"Acceptance = checklist complete"** means: Part A's sweep runs green on
   the current tree (exit 0; baseline 21 suites / 248 cases until SH-702
   adds its suite), *and* every Part B row carries a filled OBSERVED value
   with the recountable result and STATUS ☑, *and* no row is left in the
   ☐ PENDING state. Partial completion is not acceptance.
2. A row's OBSERVED may deviate from EXPECTED only if the deviation is
   accepted as a work item: record the decision in the owning ticket (or
   ISSUES.md) with a follow-up ticket ID — the deviation is then acceptable
   only until that ticket closes. Silent deviations fail the checklist.
3. Re-running: host rows are re-verified with
   `scripts/run-tests.sh <tree>` before every release; device rows must be
   observed in a single coherent session on emulator/device and re-run after
   any change to playback, library, or settings logic (and after device
   reboot, mirroring engine checklist row 10).
4. Progress snapshot (update on each device session):

| Date | Session | Automated gate | Manual rows ticked | Total rows |
| --- | --- | --- | --- | --- |
| 2026-11-10 | SH-701 charter | PASS (21 suites / 248 cases) | 0 of 22 | 22 |
| 2026-11-11 | wave-8 emulator (headless, i486) | PASS (22 suites / 256 cases) | 0 of 22 ticked — partial OBSERVED evidence only: **C3** (restart keeps managed folders + config, screenshots), **C1**-lite (re-scan of a new folder lists only .ogg), **E2** (Library page renders fully: folder list, whitelist/blacklist sections, multi-select buttons, Tracks (43)), **E1**-lite (shell launches, cover unverified). Rows A2–A5/B2-*/C2/D*/E3–E5 need touch input (FilePicker, page navigation, playback control) — headless emulator cannot drive them. Journal clean (0 warnings/errors) after the wave-8 fixes (ODR header, `import "pages"`, toast color, sailjail `Permissions=Music`). | 22 |
