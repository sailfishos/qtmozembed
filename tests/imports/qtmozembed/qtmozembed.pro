MODULENAME = QtMozilla
TARGET = qtmozembedplugin

include (../imports.pri)

QTMOZEMBED_SOURCE_PATH = $$PWD/../../../src

QT += dbus declarative script

LIBS+=-L../../../ -lqtembedwidget

INCLUDEPATH += $$QTMOZEMBED_SOURCE_PATH

SOURCES += \
        main.cpp

import.files = qmldir
import.path = $$TARGETPATH
INSTALLS += import
