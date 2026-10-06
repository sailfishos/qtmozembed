# Retain the old runtime name for binaries using the documented WebView API.
# The compatible QMozContext/QMozEngineSettings symbols and state live in v2.
TEMPLATE = lib
TARGET = qt5embedwidget
VERSION = 1.0.0
CONFIG -= qt create_pc create_prl
SOURCES += compat.cpp

RELATIVE_PATH = ..
VDEPTH_PATH = compat
include(../relative-objdir.pri)
COMPAT_OUTPUT_DIR = $$clean_path($$OUT_PWD/$$DESTDIR)
DESTDIR = $$COMPAT_OUTPUT_DIR
equals(COMPAT_OUTPUT_DIR, $$OUT_PWD): DESTDIR =

target.path = $$[QT_INSTALL_LIBS]
INSTALLS += target
