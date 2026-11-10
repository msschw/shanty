# SH-702 packaging & release lint suite, adapted from tests/license
# (SH-102): same TARGET/DESTDIR layout, same PRE_TARGETDEPS pattern for
# the core static lib so the binary relinks after component changes
# (SH-101 note). The suite itself is a repository/file lint — it links
# the core lib for pattern consistency with the license suite only.
# SPDX-License-Identifier: GPL-3.0-or-later

TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_packaging
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core
# SH-101: track the static libs so suite binaries always relink (agents
# reported stale binaries after core/adapters changes).
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a

SOURCES += tst_packaging.cpp

# SH-702: the suite scans the repository; root injected at build time
# (same pattern as tests/license/license.inc, inlined here).
SHANTY_ROOT = $$clean_path($$PWD/../..)
DEFINES += SHANTY_ROOT=\\\"$$SHANTY_ROOT\\\"
