MODULENAME = com/mozilla/qtmozembed
TARGET = qtmozembedplugin

include (../imports.pri)

QTMOZEMBED_SOURCE_PATH = $$PWD/../../../src

QT += dbus declarative script
CONFIG += mobility link_pkgconfig
MOBILITY += qtmozembed

INCLUDEPATH += $$QTMOZEMBED_SOURCE_PATH

HEADERS += \
        $$QTMOZEMBED_SOURCE_PATH/declarativedbusinterface.h \
        $$QTMOZEMBED_SOURCE_PATH/declarativemediamodel.h \
        $$QTMOZEMBED_SOURCE_PATH/declarativemediasource.h \
        $$QTMOZEMBED_SOURCE_PATH/declarativefileinfo.h \
        $$QTMOZEMBED_SOURCE_PATH/declarativewallpaper.h

SOURCES += \
        main.cpp \
        $$QTMOZEMBED_SOURCE_PATH/declarativedbusinterface.cpp \
        $$QTMOZEMBED_SOURCE_PATH/declarativemediamodel.cpp \
        $$QTMOZEMBED_SOURCE_PATH/declarativemediasource.cpp \
        $$QTMOZEMBED_SOURCE_PATH/declarativefileinfo.cpp \
        $$QTMOZEMBED_SOURCE_PATH/declarativewallpaper.cpp


import.files = qmldir
import.path = $$TARGETPATH
INSTALLS += import
