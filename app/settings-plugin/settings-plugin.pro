# SH-1008 (UX-19): the Shanty.Settings QML plugin. The per-app page the OS
# Settings app loads (Settings → Apps → Shanty) runs in jolla-settings' QML
# engine, not the app's, so the library-config view models must be
# registered for THAT engine. The plugin exposes a deliberately narrow
# singleton (ShantySettings) over the same SettingsFacade + FileIndexStore
# the app uses — same file, single write path, no second process.
#
# Device-only (built from the non-host branch of shanty.pro): it links the
# QtQml plugin surface of the target and the same static libs as the app.

TEMPLATE = lib
CONFIG += plugin c++14
CONFIG -= app_bundle

QT += qml

QMAKE_CXXFLAGS += -Wall -Wextra

TARGET = shantysettings

# Generated moc must not pollute the source tree: the license lint scans
# every *.cpp outside root-level build-* trees (same rule as app/app.pro).
MOC_DIR = $$OUT_PWD/../../build-settings-plugin-generated
# qmake does not create MOC_DIR before invoking moc here; keep a cleaned
# tree buildable (same rule as app.pro).
!exists($$MOC_DIR): system(mkdir -p $$shell_quote($$MOC_DIR))

INCLUDEPATH += $$PWD/..
INCLUDEPATH += $$PWD/../..
INCLUDEPATH += $$PWD/../../core

HEADERS += \
    SettingsPlugin.h \
    ShantySettingsSingleton.h \
    ../controllers/SettingsFacade.h

SOURCES += \
    SettingsPlugin.cpp \
    ShantySettingsSingleton.cpp \
    ../controllers/SettingsFacade.cpp

# Static libs built by the sibling SUBDIRS (link order: users first).
LIBS += -L$$OUT_PWD/../../presentation/viewmodels -lshanty-viewmodels
LIBS += -L$$OUT_PWD/../../adapters -lshanty-adapters
LIBS += -L$$OUT_PWD/../../core -lshanty-core

# Same pkg-config surface as app/app.pro: the adapters archive is a device
# build (taglib always, gstreamer in the HAVE_GSTREAMER variant). Only the
# objects we reference are pulled in, but the link line stays consistent.
CONFIG += link_pkgconfig
PKGCONFIG += taglib gstreamer-1.0

# QML module layout: <QML root>/Shanty/Settings/{qmldir,libshantysettings.so}
target.path = $$[QT_INSTALL_QML]/Shanty/Settings
INSTALLS += target

qmldir.files = qmldir
qmldir.path = $$[QT_INSTALL_QML]/Shanty/Settings
INSTALLS += qmldir
