import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared/sharedTests.js" as SharedTests

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized

    QmlMozContext {
        id: mozContext
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            appWindow.mozViewInitialized = true
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_viewtest cleanup")
        }

        function test_Test1LoadSimpleBlank()
        {
            SharedTests.shared_Test1LoadSimpleBlank()
        }
        function test_Test2LoadAboutMozillaCheckTitle()
        {
            SharedTests.shared_Test2LoadAboutMozillaCheckTitle()
        }
    }
}
