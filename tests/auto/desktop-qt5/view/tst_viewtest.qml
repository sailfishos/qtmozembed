import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

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
        onViewInitialized: appWindow.mozViewInitialized = true
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
            mozContext.dumpTS("test_Test1LoadSimpleBlank start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            webViewport.url = "about:blank";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100)
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            mozContext.dumpTS("test_Test1LoadSimpleBlank end")
        }
        function test_Test2LoadAboutMozillaCheckTitle()
        {
            mozContext.dumpTS("test_Test2LoadAboutMozillaCheckTitle start")
            webViewport.url = "about:mozilla";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.title, "The Book of Mozilla, 11:14")
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            mozContext.dumpTS("test_Test2LoadAboutMozillaCheckTitle end")
        }
    }
}
