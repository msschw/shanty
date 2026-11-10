// SH-101: the app component keeps the Sailfish 'sailfishapp' template.
# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

TARGET = shanty
TEMPLATE = app

CONFIG += sailfishapp
DEFINES += BUILD_APP

SOURCES += shanty.cpp

DISTFILES += ../qml/shanty.qml \
    ../qml/cover/CoverPage.qml \
    ../qml/pages/FirstPage.qml \
    ../qml/pages/SecondPage.qml \
    ../rpm/shanty.changes.in \
    ../rpm/shanty.changes.run.in \
    ../rpm/shanty.spec \
    ../translations/*.ts \
    ../shanty.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
TRANSLATIONS += ../translations/shanty-de.ts
