CONFIG += qt thread debug ordered create_pc create_prl no_install_prl c++11
QT += openglextensions
TARGET = qt5embedwidget
TEMPLATE = lib

# Avoid X11 header collision
DEFINES += MESA_EGL_NO_X11_HEADERS

isEmpty(VERSION) {
    GIT_TAG = $$system(git describe --tags --abbrev=0)
    GIT_VERSION = $$system(echo $$GIT_TAG | sed 's/nemo[/]//')
    isEmpty(GIT_VERSION) {
        # if you're trying to build this from a tarball, I'm sorry. but being
        # able to specify the version in just one place (git tags) is a lot less
        # error-prone and easy.
        warning("Can't find a valid git tag version, got: $$GIT_TAG")
        GIT_VERSION = 0.0.0
    }
    !isEmpty(GIT_VERSION): VERSION = $$GIT_VERSION
}

SOURCES += qmozcontext.cpp \
           qmozenginesettings.cpp \
           qmozgrabresult.cpp \
           qmozscrolldecorator.cpp \
           qmessagepump.cpp \
           EmbedQtKeyUtils.cpp \
           qmozsecurity.cpp \
           qmozview_p.cpp \
           geckoworker.cpp \
           qopenglwebpage.cpp \
           qmozwindow.cpp \
           qmozwindow_p.cpp

HEADERS += qmozcontext.h \
           qmozcontext_p.h \
           qmozenginesettings.h \
           qmozenginesettings_p.h \
           qmozgrabresult.h \
           qmozsecurity.h \
           qmozviewcreator.h \
           qmozscrolldecorator.h \
           qmessagepump.h \
           EmbedQtKeyUtils.h \
           qmozview_p.h \
           geckoworker.h \
           qmozview_defined_wrapper.h \
           qmozview_templated_wrapper.h \
           qopenglwebpage.h \
           qmozwindow.h \
           qmozwindow_p.h

SOURCES += quickmozview.cpp qmozexttexture.cpp qmozextmaterialnode.cpp
HEADERS += quickmozview.h qmozexttexture.h qmozextmaterialnode.h

include(qmozembed.pri)

RELATIVE_PATH=..
VDEPTH_PATH=src
include($$RELATIVE_PATH/relative-objdir.pri)

PREFIX = /usr

QT += quick qml

#DEFINES += Q_DEBUG_LOG

target.path = $$[QT_INSTALL_LIBS]

QMAKE_PKGCONFIG_NAME = qt5embedwidget
QMAKE_PKGCONFIG_DESCRIPTION = Model that emits process info
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_INCDIR = $$target.path
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
QMAKE_PKGCONFIG_REQUIRES = libxul

# install forwarding headers
# match only the camel case forwarding headers here
FORWARDING_HEADERS = $$system( find q*.h )

forwarding_headers.path = $$PREFIX/include
forwarding_headers.files = $$FORWARDING_HEADERS
INSTALLS += forwarding_headers target
