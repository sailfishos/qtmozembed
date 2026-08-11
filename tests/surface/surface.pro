TEMPLATE = app
TARGET = tst_qmozsurface
CONFIG += console c++1z
CONFIG -= app_bundle
QT -= gui

INCLUDEPATH += ../../src

SOURCES += tst_qmozsurface.cpp \
           ../../src/runtime/qmozsurface_p.cpp

target.path = /opt/tests/qtmozembed
INSTALLS += target
