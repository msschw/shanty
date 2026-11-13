# SH-101: test suites, one binary per ticket-friendly suite. All suites run
# on the host and execute `make check` over the whole set.
# Expected state during Phase A: suites RUN but several tests FAILED
# (asserting against unimplemented stubs). Compile errors are NOT acceptable.
# Note: deliberately NOT `CONFIG += ordered` — we want every suite to report
# its state even when earlier suites fail (Phase A red report).
# Library build order is guaranteed by the ordered top-level project; the
# runner script (scripts/run-tests.sh) runs `make` before `make check`.

TEMPLATE = subdirs

SUBDIRS = \
    license \
    track \
    library_scan \
    filesystem_scanner \
    library_store \
    queue_store \
    playlist_ops \
    m3u_parse \
    m3u_encoding \
    m3u_roundtrip \
    m3u_resolve \
    playback_queue \
    playback_controller \
    equalizer_profile \
    effect_chain \
    autoeq \
    playlist_vm \
    player_vm \
    library_vm \
    navigation \
    engine_adapter

# SH-605: settings view-model suite (appended by its wave agent).
SUBDIRS += settings_vm

# SH-702: packaging & release lint suite (spec template keys, %changelog,
# %files install-set coverage, icons, DISTFILES/translations existence,
# soft .ts-vs-QML parity).
SUBDIRS += packaging

# Wave-8 follow-up: FilePickerPage directory-listing contract (suffix
# filter semantics shared by the QML layer and the facade provider).
SUBDIRS += picker_support

# SH-902 (SH-E9): profile → device-band mapping (pure logic, core).
SUBDIRS += eq_mapping
