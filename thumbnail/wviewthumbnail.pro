TEMPLATE = lib
CONFIG += dll c++17
CONFIG -= qt debug_and_release debug_and_release_target

QT -= core gui

TARGET = wViewThumbnail
DESTDIR = $$PWD/../bin
OBJECTS_DIR = $$PWD/../build/thumbnail

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
    QMAKE_LFLAGS += /DEF:$$shell_path($$PWD/wviewthumbnail.def)
    # Keep the import library/export file out of the deployed bin folder
    QMAKE_LFLAGS += /IMPLIB:$$shell_path($$OBJECTS_DIR)/wViewThumbnail.lib
}

LIBS += -lshell32 -lole32 -loleaut32 -lshlwapi -lwindowscodecs -lgdi32 -ladvapi32 -luser32 -luuid -lkernel32

SOURCES += \
    $$PWD/wviewthumbnail.cpp
