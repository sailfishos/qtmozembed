import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: appWindow.mozViewInitialized = true
    }

    TestCase {
        id: testcaseid
        name: "tst_viewtest"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_viewtest cleanupTestCase")
        }

        function test_Test1LoadSimpleBlank()
        {
            MyScript.dumpTs("test_Test1LoadSimpleBlank start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            webViewport.url = "about:blank";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100)
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            MyScript.dumpTs("test_Test1LoadSimpleBlank end")
        }
        function test_Test2LoadAboutMozillaCheckTitle()
        {
            MyScript.dumpTs("test_Test2LoadAboutMozillaCheckTitle start")
            webViewport.url = "about:mozilla";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.title, "The Book of Mozilla, 11:14")
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            MyScript.dumpTs("test_Test2LoadAboutMozillaCheckTitle end")
        }
    }
}
