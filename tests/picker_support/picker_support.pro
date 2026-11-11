TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# Wave-8 follow-up: FilePickerPage directory-listing contract. The
# listDirectory provider is a header-only helper in the app layer
# (app/controllers/PickerSupport.h, QtCore only) so the suite compiles it
# directly — no static lib needed.
TARGET = tst_picker_support
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

SOURCES += tst_picker_support.cpp
