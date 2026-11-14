TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_tag_reader
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../../adapters
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink.
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a

# SH-1001: committed fixture files (tiny real tagged audio, generated
# once with taglib itself — see tst_tag_reader.cpp header). Fixture
# paths resolve via SHANTY_ROOT (out-of-source-build-proof — the
# QFINDTESTDATA heuristic does not see the source tree here).
SHANTY_ROOT = $$clean_path($$PWD/../..)
DEFINES += SHANTY_ROOT=\\\"$$SHANTY_ROOT\\\"

SOURCES += tst_tag_reader.cpp
