# SH-101: domain core. Pure C++ (no Qt, no QML) per design decision D1 —
# fully unit-testable on the host.

TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = shanty-core

# Headers use core-root-relative quoted includes (e.g.
# "playback/PlaybackQueue.h" from services/, "core/ports/..." style is NOT
# used here — core stays self-contained relative to $$PWD).
INCLUDEPATH += $$PWD

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
    domain/AutoEqParseResult.h \
    domain/EqualizerBandMapping.h \
    domain/TagMetadata.h \
    domain/PlaylistFile.h \
    playback/PlaybackQueue.h \
    playback/PlaybackController.h \
    ports/IPlaylistCodec.h \
    ports/ILibraryScanner.h \
    ports/ILibraryStore.h \
    ports/ITagReader.h \
    ports/ISettingsStore.h \
    ports/IAudioEngine.h \
    ports/IEffectChain.h \
    ports/IAutoEqReader.h \
    services/LibraryService.h \
    services/SettingsService.h \
    services/EffectChainService.h \
    services/PlaylistService.h

SOURCES += \
    domain/AudioFormat.cpp \
    domain/Track.cpp \
    domain/FilterRule.cpp \
    domain/Playlist.cpp \
    domain/PathResolver.cpp \
    domain/EqualizerProfile.cpp \
    domain/EqualizerBandMapping.cpp \
    domain/PlaylistFile.cpp \
    playback/PlaybackQueue.cpp \
    playback/PlaybackController.cpp \
    services/LibraryService.cpp \
    services/SettingsService.cpp \
    services/EffectChainService.cpp \
    services/PlaylistService.cpp
