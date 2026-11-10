# SH-101: adapters. Thin, framework-bound implementations of the core
# ports. Kept Qt-free at stub level so they build on the host; Phase B
# implementations may freely use Qt/QtCore where needed.

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt
# SH-203 Phase B: FilesystemScanner enumerates via QtCore (QDir/QDirIterator);
# re-enable Qt support here (stub-era rule above said Phase B may freely use
# Qt/QtCore) so qmake supplies the QtCore include paths.
QT += core
CONFIG += qt

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# SH-204 Phase B: FileIndexStore persists through QtCore (QFile/QSaveFile/
# QStandardPaths). Same order as above — overriding the stub-level "-qt".
CONFIG += qt
QT += core

TARGET = shanty-adapters

INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

VPATH = $$PWD

HEADERS += \
    m3u/M3uPlaylistCodec.h \
    filesystem/FilesystemScanner.h \
    store/FileIndexStore.h \
    autoeq/AutoEqReader.h

SOURCES += \
    m3u/M3uPlaylistCodec.cpp \
    filesystem/FilesystemScanner.cpp \
    store/FileIndexStore.cpp \
    autoeq/AutoEqReader.cpp

# SH-503: adapters/autoeq/AutoEqReader is deliberately Qt-free (plain C++
# parser per the SH-503 suite contract), so no additional qt entries are
# needed here — the ones above (QT += core / CONFIG += qt from SH-203/
# SH-204 Phase B) stay as-is.
