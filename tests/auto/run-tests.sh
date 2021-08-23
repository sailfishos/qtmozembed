#!/bin/sh

export EMBED_CONSOLE=stacktrace

if [ "$QTMOZEMBEDOBJDIR" != "" ]; then
  QMLMOZTESTRUNNER=$QTMOZEMBEDOBJDIR/tests/qmlmoztestrunner/qmlmoztestrunner
  export QML2_IMPORT_PATH=$QTMOZEMBEDOBJDIR/qmlplugin5
else
  QMLMOZTESTRUNNER=/opt/tests/qtmozembed/qmlmoztestrunner
  export QML_IMPORT_PATH=/opt/tests/qtmozembed/imports
fi

export QTTESTSROOT=${QTTESTSROOT:-"/opt/tests/qtmozembed"}
#export MOZ_LOG=EmbedLiteTrace:5,EmbedNonImpl:5,EmbedLiteApp:5,EmbedLiteView:5,EmbedLiteViewThreadParent:5

# Clean up settings
rm -rf ~/.mozilla/mozembed-testrunner/

$QMLMOZTESTRUNNER -opengl $@
exit_code=$?

# Clean up settings afterwards
rm -rf ~/.mozilla/mozembed-testrunner/

echo "exit_code $exit_code"

exit $exit_code
