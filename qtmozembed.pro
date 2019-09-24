TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = src
SUBDIRS += qmlplugin5
qmlplugin5.depends = src

isEmpty(NO_TESTS) {
  SUBDIRS += tests
}

OTHER_FILES += rpm/qtmozembed-qt5.spec
