# SH-503 Phase A: AutoEq profile import suite (tests/autoeq), adapted from
# tests/equalizer_profile/equalizer_profile.pro (same static-lib link
# pattern incl. PRE_TARGETDEPS so suite binaries relink after core/adapters
# changes). NB: PRE_TARGETDEPS includes the adapters lib because this suite
# exercises the AutoEqReader adapter (SH-503 Phase B edits it).

TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_autoeq
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes). SH-503: the adapters
# lib is tracked as well because AutoEqReader (adapters/autoeq) is the
# unit under test.
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a
PRE_TARGETDEPS += $$OUT_PWD/../../adapters/libshanty-adapters.a

SOURCES += tst_autoeq.cpp
