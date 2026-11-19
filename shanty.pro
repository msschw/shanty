# SH-101: Top-level project restructured as subdirs (ISSUES.md §1.3).
# app/ keeps the Sailfish 'sailfishapp' template configuration. On a plain
# host build (no Sailfish SDK), pass `onHost` (see scripts/run-tests.sh) to
# skip the app and build core + adapters + tests only.

TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS += core adapters presentation

adapters.depends = core
presentation.depends = core

onHost {
    SUBDIRS += tests
    tests.depends = presentation
} else {
    SUBDIRS += app
    app.depends = presentation
    # SH-1008 (UX-19): the Shanty.Settings QML plugin loaded by the OS
    # Settings app (Settings → Apps → Shanty). Device-only: it needs the
    # QtQml plugin surface of the target and links the same static libs.
    settingsplugin.subdir = app/settings-plugin
    settingsplugin.depends = presentation
    SUBDIRS += settingsplugin
}

DISTFILES += PROJECT.md ISSUES.md
