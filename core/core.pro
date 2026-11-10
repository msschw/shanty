# SH-101: domain core. Pure C++ (no Qt, no QML) per design decision D1 —
# fully unit-testable on the host.

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = shanty-core

VPATH = $$PWD

HEADERS += \
    domain/AudioFormat.h \
    domain/Track.h \
    domain/FilterRule.h \
    domain/LibraryConfig.h \
    domain/PlaylistEntry.h \
    domain/Playlist.h \
    domain/M3uParseResult.h \
    domain/PathResolver.h \
    domain/EqualizerProfile.h \
    playback/PlaybackQueue.h \
    ports/IPlaylistCodec.h \
    ports/ILibraryScanner.h \
    ports/ILibraryStore.h \
    ports/ISettingsStore.h \
    ports/IAudioEngine.h \
    ports/IEffectChain.h \
    services/LibraryService.h \
    services/SettingsService.h

SOURCES += \
    domain/AudioFormat.cpp \
    domain/Track.cpp \
    domain/FilterRule.cpp \
    domain/Playlist.cpp \
    domain/PathResolver.cpp \
    domain/EqualizerProfile.cpp \
    playback/PlaybackQueue.cpp \
    services/LibraryService.cpp \
    services/SettingsService.cpp
