# Shanty — App Size Audit (SH-702)

## Purpose

PROJECT.md (charter) defines **lean by design** as a hard product goal:
"minimal dependencies, minimal resource usage, no superfluous features".
This document turns that into an *auditable budget*: what Shanty ships,
measured against a declared ceiling, with a repeatable accounting
procedure. It supports the spec in `rpm/shanty.spec` (built in the
Sailfish SDK — no RPM tooling is run on the host; see §5).

## 1. Budget definition (the declared lean ceiling)

Shanty ships as **one Silica application**. The lean ceiling is:

| Dimension | Ceiling |
| --- | --- |
| Installed executable (`/usr/bin/shanty`) | **< 2 MB** (2 × 1024 kB) |
| Shipped data payload (`/usr/share/shanty` qml+`.qm`, icons, desktop) | included in the audit, expected ≪ 1 MB (template: qml 28 kB, icons 64 kB, translations 12 kB) |
| Runtime dependencies (RPM `Requires`) | **exactly the one below — no additions** |
| Background services / daemons | **none** (playback lives in the app process) |

Runtime dependency list — closed, charter-minimal, mirrored 1:1 in
`rpm/shanty.spec` `Requires`:

1. `sailfishsilica-qt5` (Silica UI framework; transitively pulls
   `Qt5Core`, `Qt5Qml`, `Qt5Quick` and the `sailfishapp` runtime library,
   which itself arrives via the automatic `libsailfishapp.so.1` soname
   dependency of `/usr/bin/shanty`)

The Qt/`pkgconfig(...)` entries are **BuildRequires only**: packages named
`pkgconfig(...)` are SDK-side `-devel` packages that do not exist on
devices — the first emulator install refused `rpm -Uvh` with
"pkgconfig(Qt5Core) is needed by shanty". Qt libraries are pulled in at
run time through automatic soname dependencies (verified on the emulator:
`Requires: libc.so.6 ... libsailfishapp.so.1` plus the declared
`sailfishsilica-qt5`).

Audio decode rides on this list: the engine adapter uses the Sailfish Qt
Multimedia fork (GStreamer/PulseAudio through the OS media stack, see
`docs/engine-adapter.md`), which the platform provides via
`sailfishsilica-qt5`/system libraries — no extra codec package named in
`Requires` beyond the one above. Any new dependency beyond it is a
charter deviation and needs an ISSUES.md decision record before merge.

## 2. Component inventory

Split per §1.3 of ISSUES.md (only the first three are linked into the
shipped binary; the tests component is host/SDK development material and
is **not** installed by `%files` — qmake `CONFIG+=testcase` binaries are
never part of `INSTALLS`):

| Component | Role | Links into shipping binary? |
| --- | --- | --- |
| `core` (domain + services + ports) | pure C++ logic | yes (static lib `libshanty-core.a`) |
| `adapters` (M3U codec, scanner, store, engine adapter) | framework-bound thin layers | yes (static lib `libshanty-adapters.a`) |
| `presentation` (view-models) | pure-C++ QObject facades | yes (static lib `libshanty-viewmodels.a`) |
| `app` (SailfishApp main) | entry point glue | yes (the `shanty` binary) |
| `app/qml/`, `app/icons/`, `translations/`, `app/shanty.desktop` | data + metadata | shipped under `%{_datadir}` |
| `tests/` | QUnit-test binaries per suite | **no** |

## 3. Host evidence (measured, CONFIG+=onHost, Qt 5.15.19, gcc -O2)

The `app` component needs `sailfishapp` (SDK-only), so the exact device
binary is produced in the SDK (§5). On the host, the component sizes are
evidenced by the static libraries in this ticket's private build tree
`build-SH-702/`, and by the test binaries that link those libraries end
to end (unstripped, GPLv3 debug-informed, which *over*-states the size —
the device binary is built with the SDK's default strip/release flags):

Archive (object) sizes — raw bytes, before the linker drops unused code:

| Artefact | du -b |
| --- | --- |
| `core/libshanty-core.a` | 889 956 B |
| `adapters/libshanty-adapters.a` | 636 758 B |
| `presentation/viewmodels/libshanty-viewmodels.a` | 430 126 B |

Link result evidence — test binaries integrating 2 or all 3 components
(`size` = text+data+bss, `ls -l` = file size):

| Artefact | links | `size` (dec) | file size |
| --- | --- | --- | --- |
| `tests/bin/tst_license` | core + adapters | 20 174 B (text 18 774) | 40 744 B |
| `tests/bin/tst_engine_adapter` | core + adapters (engine) | — | 40 696 B |
| `tests/bin/tst_library_vm` | core + adapters + viewmodels | 97 276 B (text 94 844) | 136 320 B |

Reading: even the *largest* integration test binary — all three
components plus QML-typed view models, **unstripped and with QtTest
itself** — is ~136 kB of file size (~95 kB text). The shipping binary is
the same three components plus thin `SailfishApp` main + resource glue,
stripped; the 2 MB ceiling leaves more than an order of magnitude of
headroom, so the honest risk is QML payload and future feature creep,
not the C++ core.

## 4. Declared accounting procedure (repeat)

Record these numbers in the release notes of every release cut:

1. **SDK build.** The spec is built in the Sailfish SDK
   (`sfdk build` / mb2 run rpmbuild; no host rpmbuild — SDK toolchain
   only). This produces `RPMS/*/shanty-0.1-1.*.rpm`.
2. **Executable.** From the unpacked RPM payload or the buildroot:
   `du -b <payload>/usr/bin/shanty` and
   `size <payload>/usr/bin/shanty` (text/data/bss split). Gate:
   text+data+bss < 2 MB.
3. **Data payload.** `du -s <payload>/usr/share/shanty`,
   `<payload>/usr/share/icons/hicolor`, `<payload>/usr/share/applications/shanty.desktop`.
4. **Dependencies.** `rpm -q --requires shanty-0.1-1.*.rpm` — the
   package deps must match the five-item §1 list (plus what rpm injects
   itself, e.g. rpmlib markers). Gate: no additions.
5. **Services.** `rpm -ql` over the payload: no systemd units, no
   `/usr/lib/systemd`, no daemons under `/usr/bin` other than `shanty`.
6. Any ceiling breach or dependency addition → new decision record in
   ISSUES.md (§2) before the release.

## 5. Build-location note

Per SH-702 ticket constraints, **no RPM build is executed on the host**:
the host build (`CONFIG+=onHost`) compiles core/adapters/presentation +
the test suites only and excludes the `app` component, and the Sailfish
side (Silica links, `qmake5_install`, RPM assembly) builds only in the
Sailfish OS SDK. This audit therefore pins *budget + procedure + host
component evidence*, with the §4 SDK measurements to be filled in at the
first release packaging run in the SDK.
