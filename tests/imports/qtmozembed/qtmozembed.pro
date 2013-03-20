MODULENAME = com/mozilla/qtmozembed
TARGET = qtmozembedplugin

include (../imports.pri)

QTMOZEMBED_SOURCE_PATH = $$PWD/../../../src

QT += dbus declarative script
CONFIG += mobility link_pkgconfig
MOBILITY += qtmozembed

LIBS+=-L../../../ -lqtembedwidget -lX11

INCLUDEPATH += $$QTMOZEMBED_SOURCE_PATH

SOURCES += \
        main.cpp

import.files = qmldir
import.path = $$TARGETPATH
INSTALLS += import
