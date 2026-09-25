QT += core testlib gui network widgets

VERSION = 1.1.0
DEFINES += MITHEINVIEW_VERSION=\\\"$$VERSION\\\"

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += tst_qviewtests.cpp

INCLUDEPATH += ../src
include( ../src/src.pri )

SOURCES -= $$absolute_path(../src/main.cpp)

win32 {
    LIBS += -lshell32 -luser32 -lole32 -loleaut32 -luuid -lshlwapi -lgdi32 -ldwmapi -lwindowsapp
    DEFINES += WIN32_LOADED
}
