# SH-604 Phase A: PlayerViewModel suite. The view-model now lives in
# libshanty-viewmodels (presented via the fold-in link entries below).
# Core = the landed PlaybackController facade (SH-403/404)
# + PlaybackQueue (SH-401); the shared FakeAudioEngine (tests/fakes) is
# reused read-only.
TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_player_vm
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes).
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a
# SH-604 fold-in: PlayerViewModel now lives in libshanty-viewmodels.
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
PRE_TARGETDEPS += $$OUT_PWD/../../presentation/viewmodels/libshanty-viewmodels.a

SOURCES += tst_player_vm.cpp
