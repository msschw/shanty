# SH-1001 — Tag metadata capability: port, taglib adapter, results

*(SH-E10 UX-12 documentation. Code: `core/ports/ITagReader.h`,
`core/domain/TagMetadata.h`, `adapters/taglib/TagLibReader.{h,cpp}`;
suite: `tests/tag_reader`.)*

Capability ticket for wave UX-D (UX.md §1 owner direction, 2026-11-14):
the media browser's Artists/Albums/Genre view modes need tag metadata
(artist/album/genre) that the app never read before (title = file
basename). Chosen reader: **taglib** — `taglib-devel` in the SDK repo
(installed with the SH-901 zypper pattern) and taglib 2.0.2 already in
the image runtime stack.

## Architecture (Dependency Inversion, ISSUES.md §1.1/§1.2)

- **Port** `core/ports/ITagReader.h` — `TagMetadata read(path)`.
- **Value type** `core/domain/TagMetadata.h` — plain UTF-8 std::string
  fields `{title, artist, album, genre}` + `durationSeconds`
  (`kUnknownDuration == -1`, shared marker with `Track`). Empty string
  = "no such tag"; tags are read-only data, never authored here.
- **Adapter** `adapters/taglib/TagLibReader.{h,cpp}` — the concrete
  reader; linked unconditionally (host AND device both carry taglib).

## Decisions (SH-D1001-*)

- **SH-D1001-1 — clean miss, never a throw.** An absent, unreadable,
  unsupported or unparseable file yields an *empty* `TagMetadata`
  (`isEmpty()`), never an exception. `isValid()` is checked BEFORE any
  tag access — taglib returns NULL tag pointers for unparseable files.
  Callers decide fallbacks (e.g. title = basename, SH-1003's catalogue).
- **SH-D1001-2 — UTF-8 boundary.** Strings cross as UTF-8
  (`toCString(true)`). Fixture-generation lesson (see below): taglib's
  `String(const std::string&)` ctor defaults to **Latin-1** — anything
  that *writes* tags from raw bytes must mark them UTF-8 explicitly.
- **SH-D1001-3 — taglib 2.x union API only.** The SDK target and the
  image carry 2.0.2, the host carries 2.3.2; the union surface
  (`FileRef` / `Tag` / `AudioProperties::lengthInSeconds()`) is
  identical across both. No format-specific classes in the adapter.
- **SH-D1001-4 — duration is audio data, not a tag.** It comes from the
  audio properties and is present even for a fully untagged file.
- **SH-D1001-5 — read at scan time.** The library store persists only
  the config today (tracks are rescanned live), so tags are re-read at
  scan/catalogue time — no store schema change in this ticket. The
  composition (scan + tags → catalogue) is SH-1003.

## Test fixtures (committed, `tests/fixtures/tags/`)

Tiny real 4 s files (tone samples from the emulator), tagged once with
taglib on the host by a one-shot generator (NOT committed — the write
path is not part of the app; regenerate with tagtool if ever needed):

| file | format | tags |
| --- | --- | --- |
| `tagged.mp3` | MPEG + ID3v2 | full set (ASCII) |
| `tagged.ogg` | Vorbis comments | full set, title `Café Deluxe` (non-ASCII round trip) |
| `partial.flac` | FLAC + Xiph | title + artist only (album/genre empty) |
| `untagged.mp3` | MPEG, no tags | all empty; duration 4 s from properties |

Fixture paths resolve via `SHANTY_ROOT` (the `QFINDTESTDATA` heuristic
does not see the source tree in out-of-source builds).

## Host results (sweep at Phase B commit)

`TestTagReader` 10/10: full reads (mp3/ogg), non-ASCII round trip
(byte-exact: 12 UTF-8 bytes), partial read (absent tags = empty, not
errors), untagged read (duration from properties), duration for all
three formats, clean miss for a missing file and a directory path.
`TestPackaging` 10/10 incl. `rpmSpecDeclaresTaglibDependencies`
(`Requires: taglib` — plain package name, the pkgconfig() runtime ban
stands; `BuildRequires: pkgconfig(taglib)`). Sweep: 26 suites / 359
cases, 0 failed.

## Device results (emulator, 2026-11-14)

Full in-tree artifact clean → SDK build green against taglib 2.0.2 →
`RPMS/shanty-0.1-1.i486.rpm`. Dependency contract verified
(`rpm -qp --requires`): explicit `taglib` + `gstreamer1.0-plugins-good`,
automatic `libgstreamer-1.0.so.0` soname, zero QtMultimedia. RPM
installed over the image (root SSH + `rpm -Uvh --force`; `pkcon
install-local` declined the simulation transaction headless — use root
rpm on this emulator). App relaunch: session restored (Player, "Track 2
of 2 (paused)", inline queue intact) — no regression from the new
dependency.

Note: the app binary does not yet *call* the reader (the composition
lands with SH-1003), so `libtag.so` is not an automatic soname
dependency of the binary yet — the explicit `Requires: taglib` is the
contractual guarantee ahead of that.

## Scope guard

- Read-only: no tag writing (UX.md §5 scope decision).
- No artwork (revisit only by charter amendment).
