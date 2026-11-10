# SH-101: adapters. Thin, framework-bound implementations of the core
# ports. Kept Qt-free at stub level so they build on the host; Phase B
# implementations may freely use Qt/QtCore where needed.

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = shanty-adapters

INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

VPATH = $$PWD

HEADERS += \
    m3u/M3uPlaylistCodec.h \
    filesystem/FilesystemScanner.h \
    store/FileIndexStore.h

SOURCES += \
    m3u/M3uPlaylistCodec.cpp \
    filesystem/FilesystemScanner.cpp \
    store/FileIndexStore.cpp
