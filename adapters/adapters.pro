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

# SH-402: AudioEngineAdapter (new module adapters/engine). Host build stays
# QtCore-only (documented stub variant, SH-D402-2); the Sailfish QtMultimedia
# backend is enabled ONLY where the multimedia module exists (Sailfish SDK/
# device). Two guards, recorded as decisions:
#   - `onHost` keeps the device backend off the host build entirely (a plain
#     host Qt may still carry a runtime multimedia module; the adapter's
#     device variant must follow the shanty.pro app/ split: SDK-only);
#   - `qtHaveModule(multimedia)` trips inside any non-host target.
# Both flags are kept in the SDK/device build: CONFIG += HAVE_QTMULTIMEDIA
# marks the build config (as requested for this ticket), and
# DEFINES += HAVE_QTMULTIMEDIA is what actually makes the #ifdef in the
# sources resolve in the toolchain.
!onHost {
    qtHaveModule(multimedia) {
        QT += multimedia
        CONFIG += HAVE_QTMULTIMEDIA
        DEFINES += HAVE_QTMULTIMEDIA
    }
}

HEADERS += \
    engine/AudioEngineAdapter.h

SOURCES += \
    engine/AudioEngineAdapter.cpp

