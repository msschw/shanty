# SH-101: the Sailfish 'sailfishapp' application component, extended for
# SH-E6 Phase B (SH-601..605): the QObject facade layer (app/controllers/)
# bridges the pure-C++ view models/services to QML, and the template QML
# pages are replaced by the Library/Player/Playlist/Settings pages plus the
# shared FilePickerPage/PromptPage dialogs.
#
# Link order (GNU ld: users before providers): the app's own objects use
# the view models (shanty-viewmodels), which use core services
# (shanty-core); the adapters (shanty-adapters) provide the concrete
# port implementations and reference QtCore only.
# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

TARGET = shanty
TEMPLATE = app

CONFIG += sailfishapp
# qmake lazy-load quirk (SH-903 device build, diagnosed on the emulator):
# CONFIG-flag .prf files load at the END of .pro parsing, in alphabetical
# order — link_pkgconfig.prf runs BEFORE sailfishapp.prf, so once we set
# CONFIG += link_pkgconfig (below, for gst), the prf's own
# "PKGCONFIG += sailfishapp" lands too late and -lsailfishapp silently
# vanishes from the link line. Declaring it during parsing fixes the
# resolution order.
PKGCONFIG += sailfishapp

DEFINES += BUILD_APP

CONFIG += c++14
QMAKE_CXXFLAGS += -Wall -Wextra

# The controller headers use Q_OBJECT, so qmake generates moc_*.cpp
# companions. sfdk builds IN-TREE, which would drop those generated files
# next to the sources — where git and the license lint (tests/license scans
# every *.cpp outside build-* trees) would see them. Route the generated
# moc output into a top-level build-* directory instead: gitignored
# (build-*/) and skipped by the license suite's artifact rule. In a private
# out-of-tree build the path stays inside the private tree.
MOC_DIR = $$OUT_PWD/../build-app-generated

# Repo root include paths: the repo root (core/, adapters/, presentation/
# header layout) plus the core root (core headers use core-relative includes
# like "playback/PlaybackQueue.h" — same pattern as presentation/viewmodels).
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD/../core

HEADERS += \
    controllers/NavigationFacade.h \
    controllers/LibraryFacade.h \
    controllers/PlaylistFacade.h \
    controllers/PlayerFacade.h \
    controllers/SettingsFacade.h \
    controllers/PickerSupport.h

SOURCES += shanty.cpp \
    controllers/NavigationFacade.cpp \
    controllers/LibraryFacade.cpp \
    controllers/PlaylistFacade.cpp \
    controllers/PlayerFacade.cpp \
    controllers/SettingsFacade.cpp

# Static libs built by the sibling SUBDIRS (ordered top-level project;
# app.depends = presentation at the top level).
LIBS += -L$$OUT_PWD/../presentation/viewmodels -lshanty-viewmodels
LIBS += -L$$OUT_PWD/../adapters -lshanty-adapters
LIBS += -L$$OUT_PWD/../core -lshanty-core

# The adapters static lib's device variant (HAVE_GSTREAMER, see
# adapters.pro) references gstreamer-1.0 symbols; the gst runtime must be
# linked into the app binary as well — same conditional as the adapter's
# (host builds never reach this branch; see SH-D402-2). SH-903 (SH-E9):
# the Qt Multimedia module is no longer linked.
!onHost {
    CONFIG += link_pkgconfig
    PKGCONFIG += gstreamer-1.0
}

DISTFILES += qml/shanty.qml \
    qml/cover/CoverPage.qml \
    qml/pages/LibraryPage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/PlayerPage.qml \
    qml/pages/PlaylistPage.qml \
    qml/pages/FilePickerPage.qml \
    qml/pages/PromptPage.qml \
    ../rpm/shanty.changes.in \
    ../rpm/shanty.changes.run.in \
    ../rpm/shanty.spec \
    ../translations/*.ts \
    shanty.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# Template layout note: qml/, shanty.desktop and icons/<size>/ live inside
# app/ (moved from the repo root during the SDK bring-up), matching
# sailfishapp.prf's resolved paths — the prf installs qml →
# /usr/share/$TARGET, the desktop file → /usr/share/applications and the
# icons → /usr/share/icons/hicolor/<size>/apps. Update tests/packaging
# (whose path contract asserted the old root layout) together with this.

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
TRANSLATIONS += ../translations/shanty-de.ts
