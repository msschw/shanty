# SH-E6: view-model component. Pure C++ (no Qt) like the domain core and
# the application facade (PlaybackController). Wave-4 agents compiled their
# view-model sources directly into their suite binaries (ownership rule);
# the epic owner folded the landed files here afterwards (wave-3 pattern).
# Suite .pro files must NOT compile these sources again (duplicate symbols).
TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = shanty-viewmodels

# Include roots: repo root ("presentation/..." includes) and core root
# ("playback/...", "services/...", "ports/..." style in the view models).
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/../../core
VPATH = $$PWD

HEADERS += \
    NavigationViewModel.h \
    LibraryViewModel.h \
    PlaylistViewModel.h \
    PlayerViewModel.h \
    SettingsViewModel.h

SOURCES += \
    NavigationViewModel.cpp \
    LibraryViewModel.cpp \
    PlaylistViewModel.cpp \
    PlayerViewModel.cpp \
    SettingsViewModel.cpp
