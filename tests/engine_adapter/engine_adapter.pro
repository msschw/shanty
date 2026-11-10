TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# SH-402: AudioEngineAdapter host suite (Phase A limited scope, per ticket:
# adapter-construction sanity + D3 URL-mapping sanity only). Adapted from
# tests/playback_controller/playback_controller.pro, keeping the PRE_TARGETDEPS
# pattern so the binary relinks after core/adapters changes (SH-101 note).
# The adapters static lib is additionally tracked because this ticket's
# adapter lives inside it (both PDF lines below).
TARGET = tst_engine_adapter
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../../adapters
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes).
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a
PRE_TARGETDEPS += $$OUT_PWD/../../adapters/libshanty-adapters.a

SOURCES += tst_engine_adapter.cpp
