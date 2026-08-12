TEMPLATE = app
TARGET = tst_qmozsurface
CONFIG += console c++1z
CONFIG -= app_bundle
QT -= gui

INCLUDEPATH += stubs ../../src

SOURCES += tst_qmozsurface.cpp \
           ../../src/runtime/qmozchromesession_p.cpp \
           ../../src/runtime/qmozframestream_p.cpp \
           ../../src/runtime/qmozsurface_p.cpp \
           ../../src/backends/embedlite/embedlitechromesession_p.cpp \
           ../../src/backends/embedlite/embedlitesurface_p.cpp

HEADERS += stubs/mozilla/embedlite/EmbedLiteApp.h \
           stubs/mozilla/embedlite/EmbedLiteChromeSession.h \
           stubs/mozilla/embedlite/EmbedLiteWindow.h

target.path = /opt/tests/qtmozembed
INSTALLS += target
