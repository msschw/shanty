# Shanty — Project Charter

## Overview

This workspace hosts **Shanty**, a Sailfish OS application built from a blank
application template. The goal is a media player that is as lean as possible —
minimal dependencies, minimal resource usage, no superfluous features — while
providing reliable local audio playback.

## Goals

- **Lean by design.** Keep the footprint small: a simple UI, few dependencies,
  and no background services beyond what playback requires.
- **Local playback.** Play audio files stored on the device. No streaming,
  no network features.
- **Manageable local media library.** The set of folders the application
  scans is user-managed: folders can be added to or removed from the
  library, and individual folders or files can be whitelisted (included)
  or blacklisted (excluded) from the library and playback.
- **Audio format support.** All audio files in the following formats:
  - MP3
  - OGG (Vorbis)
  - FLAC
- **M3U playlist import, editing, and export.** Import existing `.m3u` /
  `.m3u8` playlists, play their contents, edit them (reorder, add, or remove
  entries), and export playlists to disk in the M3U format — including
  saving edits to the original file and exporting the current playlist to a
  new M3U file.

## User Interface / Views

The application provides dedicated views:

- **Library view** for managing the media library: add/remove folders,
  manage the whitelist/blacklist of folders and files.
- **Settings view** for application preferences: general player settings,
  equalizer, AutoEq profiles, and further configuration (e.g. audio
  output options).
- **Player view** showing the currently configured/playing playlist,
  together with transport controls: play/pause, next, and previous.

## Reference Implementation (Upstream)

When in doubt about expected features or behaviour, the upstream Android
application serves as the reference, since it already ships all expected
features:

- **Rhythm** — <https://github.com/cromaguy/Rhythm>
  (GPL-3.0, Kotlin/Media3, open-source Android music player)

Rhythm is the arbiter for feature scope and interaction design questions
(e.g. how the equalizer, AutoEq profiles, queue handling, and library
management are expected to behave). Shanty does **not** aim for 1:1 parity:
Rhythm features that belong to the Android platform (Material theming,
foldable postures, widgets) or fall outside this project's goals (streaming,
lyrics, stats) are out of scope. Where a feature is adopted, it is
re-interpreted for a lean Sailfish OS application.

> **License note:** Rhythm is GPL-3.0 and Shanty will be licensed GPL-3.0 as
> well. This makes carrying over code (not just behaviour) feasible where
> desired, provided the GPL terms and attribution are respected.

## Non-Goals

- Online streaming or any network-dependent functionality
  (e.g. Rhythm's "Go Mode" server playback and downloads).
- Video playback.
- Online playlist sources or sync (editing applies to local/imported playlists only).
- Metadata tag editing.
- Android-platform-specific Rhythm features: synced lyrics, playback stats,
  home-screen widgets, foldable-posture layouts, Material You theming.

## License

Shanty is free software, licensed under the
**GNU General Public License v3.0** — the same license as the upstream
Rhythm reference implementation. Any code carried over from Rhythm must
retain its copyright notices and remain compatible with GPL-3.0.

## Engineering Principles

### SOLID

Development strictly adheres to the SOLID design principles:

- **S — Single Responsibility:** every class/module has exactly one reason to
  change. Playback, playlist parsing, and file discovery are separate concerns.
- **O — Open/Closed:** components are extensible (e.g. new audio formats or
  playlist formats) without modifying existing, working code.
- **L — Liskov Substitution:** implementations of a contract (e.g. a decoder
  or playlist-reader interface) must be substitutable without breaking callers.
- **I — Interface Segregation:** expose small, focused interfaces instead of
  large, monolithic ones.
- **D — Dependency Inversion:** high-level logic (playback control, playlist
  handling) depends on abstractions, not on concrete framework or backend
  details. This also keeps the core testable off-device.

### Test-Driven Development (TDD)

- New behaviour is written test-first: define the test, watch it fail,
  implement, refactor.
- The playlist importer and playback core must be covered by unit tests;
  framework-dependent code (QML, Sailfish integration) is isolated behind
  abstractions so the logic can be tested on the host.
- Tests run as part of every change; regressions are fixed before features
  are added.

## Repository Layout (Sailfish OS template)

```
qml/         QML UI code
src/         C++ sources (application entry point, core logic)
rpm/         Packaging (spec file, systemd/user settings)
icons/       Application and cover icons
translations/ Localization files
shanty.pro   Qt project file
shanty.desktop  Application desktop entry
```

## Scope of Initial Work

1. Wire up unit testing in the build so the TDD workflow can start immediately.
2. Implement the M3U (`.m3u`/`.m3u8`) playlist importer, editor, and
   exporter as isolated, tested components (parse → model → serialize
   round trips).
3. Implement local audio playback for MP3, OGG, and FLAC.
4. Implement the media library management: add/remove folders, whitelist
   and blacklist folders/files, all derived from persisted, tested settings.
5. Add the dedicated views: Library (manage folders, whitelist/blacklist),
   Settings (player settings, equalizer, AutoEq profiles), and Player
   (current/playing playlist with play/pause, next, previous controls).
