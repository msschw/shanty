TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# SH-602: library view-model suite. LibraryViewModel lives in
# presentation/viewmodels (libshanty-viewmodels) after the wave-4 fold-in;
# core services come via the PRE_TARGETDEPS static libs
# (tests/library_scan pattern).
TARGET = tst_library_vm
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../../presentation
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes).
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a
# SH-602 fold-in: LibraryViewModel now lives in libshanty-viewmodels.
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
PRE_TARGETDEPS += $$OUT_PWD/../../presentation/viewmodels/libshanty-viewmodels.a

SOURCES += tst_library_vm.cpp

