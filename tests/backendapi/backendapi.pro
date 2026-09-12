TEMPLATE = app
TARGET = tst_backendapi
CONFIG += console c++1z
CONFIG -= app_bundle qt

INCLUDEPATH += ../../src

SOURCES += tst_backendabi.c \
           tst_backendapi.cpp \
           ../../src/runtime/backendapi_p.cpp \
           ../../src/backends/embedlite/embedlitebackendentry_p.cpp

target.path = /opt/tests/qtmozembed
INSTALLS += target
