
isEmpty(OBJ_PATH) {
  message(OBJ_PATH not defined)
  CONFIG += link_pkgconfig
  SDK_HOME=$$system(pkg-config --variable=sdkdir libxul-embedding)
  GECKO_LIB_DIR = $$SDK_HOME/lib
  GECKO_INCLUDE_DIR = $$SDK_HOME/include
  BIN_DIR=$$replace(SDK_HOME, -devel-, -)
  message($$BIN_DIR - binary dir)
} else {
  CONFIG += link_pkgconfig
  message(OBJ_PATH defined $$OBJ_PATH)
  GECKO_LIB_DIR = $$OBJ_PATH/dist/sdk/lib
  GECKO_INCLUDE_DIR = $$OBJ_PATH/dist/include
  BIN_DIR=$$OBJ_PATH/dist/bin
  message($$BIN_DIR - binary dir)
}

CONFIG += \
    egl

QMAKE_CXXFLAGS += -I $$GECKO_INCLUDE_DIR -include mozilla-config.h
unix:QMAKE_CXXFLAGS += -fno-short-wchar -fPIC
DEFINES += XPCOM_GLUE=1 XPCOM_GLUE_USE_NSPR=1 MOZ_GLUE_IN_PROGRAM=1

!isEmpty(ENABLE_GLX) {
    DEFINES += ENABLE_GLX
}

#INCLUDEPATH += $$GECKO_INCLUDE_DIR/nspr /usr/include/nspr4
contains(CONFIG, with-system-nspr) {
    INCLUDEPATH += $$system(pkg-config --cflags-only-I nspr)
} else {
    INCLUDEPATH += $$system(pkg-config --cflags-only-I libxul)
}

LIBS += -L$$GECKO_LIB_DIR -lxpcomglue -Wl,--whole-archive -lmozglue
LIBS += -Wl,--no-whole-archive -rdynamic -ldl
PKGCONFIG += nspr pixman-1 systemsettings libxul

DEFINES += BUILD_GRE_HOME=\"\\\"$$BIN_DIR\\\"\"

# Copy default mozilla flags to avoid some gcc warnings
*-g++*: QMAKE_CXXFLAGS += -Wno-attributes
*-g++*: QMAKE_CXXFLAGS += -Wno-ignored-qualifiers
*-g++*: QMAKE_CXXFLAGS += -pedantic
*-g++*: QMAKE_CXXFLAGS += -Wall
*-g++*: QMAKE_CXXFLAGS += -Wno-unused-parameter
*-g++*: QMAKE_CXXFLAGS += -Wpointer-arith
*-g++*: QMAKE_CXXFLAGS += -Woverloaded-virtual
*-g++*: QMAKE_CXXFLAGS += -Werror=return-type
*-g++*: QMAKE_CXXFLAGS += -Wtype-limits
*-g++*: QMAKE_CXXFLAGS += -Wempty-body
*-g++*: QMAKE_CXXFLAGS += -Wno-ctor-dtor-privacy
*-g++*: QMAKE_CXXFLAGS += -Wno-format
*-g++*: QMAKE_CXXFLAGS += -Wno-overlength-strings
*-g++*: QMAKE_CXXFLAGS += -Wno-invalid-offsetof
*-g++*: QMAKE_CXXFLAGS += -Wno-variadic-macros
*-g++*: QMAKE_CXXFLAGS += -Wno-long-long
*-g++*: QMAKE_CXXFLAGS += -Wno-psabi
