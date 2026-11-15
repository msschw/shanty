# SH-1003 (SH-E10 UX-14): library catalogue view-model suite — grouping of
# the scanned library over tag metadata for the media browser view modes.
TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = tst_library_catalogue
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

# LibraryCatalogueViewModel lives in libshanty-viewmodels (same link order
# rule as the other view-model suites: core linked AFTER the archive).
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
PRE_TARGETDEPS += $$OUT_PWD/../../presentation/viewmodels/libshanty-viewmodels.a
LIBS += -L$$OUT_PWD/../../core -lshanty-core
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a

SOURCES += tst_library_catalogue.cpp
HEADERS += FakeTagReader.h \
    FakePersistingLibraryStore.h \
    RecordingScanner.h
