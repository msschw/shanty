TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# SH-601: NavigationViewModel page-stack state machine (pure C++, no Qt,
# std::function hooks like the PlaybackController facade).
TARGET = tst_navigation
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes).
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a
# SH-601 fold-in: NavigationViewModel now lives in libshanty-viewmodels.
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
PRE_TARGETDEPS += $$OUT_PWD/../../presentation/viewmodels/libshanty-viewmodels.a

# Wave-4 fold: the state machine lives in libshanty-viewmodels now
# (pure C++ with no external dependency). SOURCES line removed at fold-in.
SOURCES += tst_navigation.cpp

