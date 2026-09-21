TARGET = wView
VERSION = 1.1.0

QT += core gui network widgets svg

TEMPLATE = app

QMAKE_PROJECT_DEPTH = 0

# allows use of the version string elsewhere (WVIEW_VERSION is a quoted string literal)
DEFINES += WVIEW_VERSION=\\\"$$VERSION\\\"

# build folder organization
DESTDIR = bin
OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build
RCC_DIR = build

CONFIG -= debug_and_release debug_and_release_target

# enable c++17
CONFIG += c++17

# Print if this is a debug or release build
CONFIG(debug, debug|release) {
    message("This is a debug build")
} else {
    message("This is a release build")
}

# Check nightly variable
# to use: qmake NIGHTLY=VERSION
!isEmpty(NIGHTLY) {
    message("This is nightly $$NIGHTLY")
    DEFINES += "NIGHTLY=$$NIGHTLY"
}

# Windows specific stuff
win32 {
    # To build without win32: qmake CONFIG+=NO_WIN32
    !CONFIG(NO_WIN32) {
        LIBS += -lshell32 -luser32 -lole32 -loleaut32 -luuid -lshlwapi -lgdi32 -ldwmapi -lwindowsapp
        DEFINES += WIN32_LOADED
        message("Linked to win32 api")
    }

    RC_ICONS = "dist/win/wView.ico"
    QMAKE_TARGET_COPYRIGHT = "Copyright \\251 2026 jurplel and wView contributors"
    QMAKE_TARGET_DESCRIPTION = "wView"
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Ban usage of Qt's built in foreach utility for better code style
DEFINES += QT_NO_FOREACH

include(src/src.pri)

# Translations are compiled to .qm files and shipped by the installer for the language chosen
# during installation. Only that language is installed, and nothing is embedded in the executable.
CONFIG += lrelease

TRANSLATIONS += $$files(i18n/wView_*.ts)

lupdate_only {
    TRANSLATIONS += i18n/template.ts
}

RESOURCES += \
    resources/resources.qrc
