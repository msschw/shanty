TEMPLATE = app

QT = testlib
QT += core

CONFIG += testcase console
CONFIG -= app_bundle

CONFIG += c++14 strict
QMAKE_CXXFLAGS += -Wall -Wextra

# SH-605: the settings view-model (profile CRUD via SettingsService, apply
# via EffectChainService, AutoEq import via IAutoEqReader) on local fakes
# (FakeSettingsStore / FakeAutoEqReader) + the shared FakeEffectChain.
TARGET = tst_settings_vm
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD/../../core
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD

# SH-605 fold-in: SettingsViewModel now lives in libshanty-viewmodels.
# Archive order matters: the settings view model applies profiles through
# EffectChainService (libshanty-core), so core must be linked AFTER the
# view-models archive.
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
PRE_TARGETDEPS += $$OUT_PWD/../../presentation/viewmodels/libshanty-viewmodels.a
LIBS += -L$$OUT_PWD/../../core -lshanty-core
PRE_TARGETDEPS += $$OUT_PWD/../../core/libshanty-core.a

# Wave-4 fold: SettingsViewModel lives in libshanty-viewmodels now
# (see the link entries above); sources must NOT be compiled here again.
SOURCES += tst_settings_vm.cpp
HEADERS += FakeSettingsStore.h \
    FakeAutoEqReader.h
