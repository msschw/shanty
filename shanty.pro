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
}

DISTFILES += PROJECT.md ISSUES.md
