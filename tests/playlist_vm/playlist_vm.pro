# SH-603 Phase A/B: playlist view-model suite (Epic SH-E6), adapted from
# tests/playback_controller/playback_controller.pro. Wave-4 fold: the two
# units landed during the wave with suite-level direct compiles and now
# live in their component libs — PlaylistService in libshanty-core,
# PlaylistViewModel in libshanty-viewmodels (link entries below); compile
# them here again and you get duplicate symbols.

TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# SH-603: PlaylistViewModel facade driving a scriptable FakePlaylistCodec.
TARGET = tst_playlist_vm
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes).
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a
# SH-603 fold-in: PlaylistService lives in libshanty-core now,
# PlaylistViewModel in libshanty-viewmodels.
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
PRE_TARGETDEPS += $$OUT_PWD/../../presentation/viewmodels/libshanty-viewmodels.a

# Direct-compile ownership (wave-3 pattern): the suite compiles its two
# units until the epic owner folds them (see note above).
SOURCES += tst_playlist_vm.cpp
