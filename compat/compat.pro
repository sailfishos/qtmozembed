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

# There are deliberately no duplicated QtMoz class definitions in this DSO.
# Keep the dependency even though the forwarding DSO has no undefined symbols.
LIBS += -L$$OUT_PWD/../$$OBJ_BUILD_PATH/src \
        -Wl,--no-as-needed -l:libqt5embedwidget.so.2 -Wl,--as-needed

# Never install the unversioned linker name: new builds must link against v2.
compat.path = $$[QT_INSTALL_LIBS]
compat.files = $$COMPAT_OUTPUT_DIR/libqt5embedwidget.so.1 \
               $$COMPAT_OUTPUT_DIR/libqt5embedwidget.so.1.0 \
               $$COMPAT_OUTPUT_DIR/libqt5embedwidget.so.1.0.0
compat.CONFIG += no_check_exist
INSTALLS += compat
