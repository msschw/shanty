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

# SH-E6 Phase B (SH-605 wiring): adapters/settings/QSettingsSettingsStore is
# the concrete ISettingsStore for the application layer. QtCore-only
# (QSettings/QStandardPaths), host-compilable — no extra qt entries beyond
# the ones above.
HEADERS += \
    m3u/M3uPlaylistCodec.h \
    filesystem/FilesystemScanner.h \
    store/FileIndexStore.h \
    store/QueueStateStore.h \
    autoeq/AutoEqReader.h \
    settings/QSettingsSettingsStore.h

SOURCES += \
    m3u/M3uPlaylistCodec.cpp \
    filesystem/FilesystemScanner.cpp \
    store/FileIndexStore.cpp \
    store/QueueStateStore.cpp \
    autoeq/AutoEqReader.cpp \
    settings/QSettingsSettingsStore.cpp

# SH-503: adapters/autoeq/AutoEqReader is deliberately Qt-free (plain C++
# parser per the SH-503 suite contract), so no additional qt entries are
# needed here — the ones above (QT += core / CONFIG += qt from SH-203/
# SH-204 Phase B) stay as-is.

# SH-402: AudioEngineAdapter (new module adapters/engine). Host build stays
# QtCore-only (documented stub variant, SH-D402-2); the Sailfish GStreamer
# backend is enabled ONLY in the SDK/device build (the host has no gst dev
# install). SH-903 (SH-E9): the Qt Multimedia module is GONE — the device
# engine rides GStreamer directly (playbin + audio-filter=equalizer-10bands,
# docs/eq-investigation.md). The gst core headers/libs are the whole compile
# surface (elements by factory name at runtime; no plugins-base API).
#   - `onHost` keeps the device backend off the host build entirely;
#   - CONFIG += HAVE_GSTREAMER marks the build config, and
#     DEFINES += HAVE_GSTREAMER is what actually makes the #ifdef in the
#     sources resolve in the toolchain.
!onHost {
    CONFIG += link_pkgconfig
    PKGCONFIG += gstreamer-1.0
    CONFIG += HAVE_GSTREAMER
    DEFINES += HAVE_GSTREAMER
}

HEADERS += \
    engine/AudioEngineAdapter.h \
    taglib/TagLibReader.h

SOURCES += \
    engine/AudioEngineAdapter.cpp \
    taglib/TagLibReader.cpp

# SH-1001 Phase A: TagLibReader is deliberately taglib-free at stub
# level (the whole adapters lib stays linkable without taglib). SH-1001
# Phase B links taglib via pkg-config here (host AND device — both
# carry the lib).

