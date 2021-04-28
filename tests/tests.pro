TEMPLATE = subdirs

SUBDIRS = qmlmoztestrunner

test.files = auto/desktop-qt5/basicinput/tst_inputtest.qml \
    auto/desktop-qt5/basicscroll/tst_scrolltest.qml \
    auto/desktop-qt5/basicview/tst_basicview.qml \
    auto/desktop-qt5/context/tst_basicmozcontext.qml \
    auto/desktop-qt5/downloadmgr/tst_downloadmgr.qml \
    auto/desktop-qt5/favicons/tst_favicon.qml \
    auto/desktop-qt5/linksactivation/tst_activatelinks.qml \
    auto/desktop-qt5/multitouch/tst_multitouch.qml \
    auto/desktop-qt5/newviewrequest/tst_newviewrequest.qml \
    auto/desktop-qt5/passwordmgr/tst_passwordmgr.qml \
    auto/desktop-qt5/promptbasic/tst_prompt.qml \
    auto/desktop-qt5/runjavascript/* \
    auto/desktop-qt5/searchengine/tst_searchengine.qml \
    auto/desktop-qt5/selection/tst_selection.qml \
    auto/desktop-qt5/viewbasicapi/tst_viewbasicapi.qml \
    auto/desktop-qt5/view/tst_viewtest.qml \

shared.files = auto/shared/componentCreation.js \
    auto/shared/ViewComponent.qml \
    auto/shared/downloadmgr/tt.bin \
    auto/shared/favicons/favicon.html \
    auto/shared/multitouch/touch.html \
    auto/shared/newviewrequest/newwin.html \
    auto/shared/passwordmgr/subtst_notifications_1.html \
    auto/shared/promptbasic/prompt.html \
    auto/shared/searchengine/test.xml

OTHER_FILES = $$test.files $$shared.files components/* components/tests/content/*

auto.files = auto/*
auto.path = /opt/tests/qtmozembed/auto

components.files = components/*
components.path = /opt/tests/qtmozembed/components

definition.files = test-definition/tests.xml
definition.path = /opt/tests/qtmozembed/test-definition

INSTALLS += auto definition components
