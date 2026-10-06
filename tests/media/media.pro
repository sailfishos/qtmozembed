TEMPLATE = app
TARGET = tst_qmozmedia
CONFIG += console c++11 testcase
QT = core testlib
INCLUDEPATH += ../../src
SOURCES += tst_qmozmedia.cpp ../../src/qmozmediacontroller.cpp
HEADERS += ../../src/qmozmediacontroller_p.h

target.path = /opt/tests/qtmozembed
INSTALLS += target
