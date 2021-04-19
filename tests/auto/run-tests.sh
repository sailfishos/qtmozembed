#!/bin/sh

QTVER=5

export LANG=en_GB.UTF-8
export EMBED_CONSOLE=stacktrace

# Create a temporary DBus session to isolate us from the normal environment.
export `dbus-launch`
if [ "$QTMOZEMBEDOBJDIR" != "" ]; then
  QMLMOZTESTRUNNER=$QTMOZEMBEDOBJDIR/tests/qmlmoztestrunner/qmlmoztestrunner
  export QML2_IMPORT_PATH=$QTMOZEMBEDOBJDIR/qmlplugin5
else
  QMLMOZTESTRUNNER=/opt/tests/qtmozembed/qmlmoztestrunner
  export QML_IMPORT_PATH=/opt/tests/qtmozembed/imports
fi
CURDIR=$PWD
export QTTESTSROOT=${QTTESTSROOT:-"/opt/tests/qtmozembed"}
if [ "$QTTESTSLOCATION" != "" ]; then
  cd $QTTESTSLOCATION
fi
export QTTESTSLOCATION=${QTTESTSLOCATION:-"/opt/tests/qtmozembed/auto/mer-qt$QTVER"}

#export MOZ_LOG=EmbedLiteTrace:5,EmbedNonImpl:5,EmbedLiteApp:5,EmbedLiteView:5,EmbedLiteViewThreadParent:5

# Clean up settings
rm -rf ~/.mozilla/mozembed-testrunner/

$QMLMOZTESTRUNNER -opengl $@
exit_code=$?
cd $CURDIR
kill $DBUS_SESSION_BUS_PID

# Clean up settings afterwards
rm -rf ~/.mozilla/mozembed-testrunner/

exit $exit_code
