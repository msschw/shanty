TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_library_scan
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

LIBS += -L$$OUT_PWD/../../adapters -L$$OUT_PWD/../../core -lshanty-adapters -lshanty-core

SOURCES += tst_library_scan.cpp
