TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_m3u_roundtrip
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes). Wave-8b: the
# adapters lib was missing here and the suite linked a stale
# libshanty-adapters after the decodeUtf8Points fix.
PRE_TARGETDEPS += $$OUT_PWD/../../adapters/libshanty-adapters.a
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a

SOURCES += tst_m3u_roundtrip.cpp
