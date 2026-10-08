import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    name: testcaseid.name

    QmlMozView {
        id: webViewport

        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: appWindow.mozViewInitialized = true
    }

    SignalSpy {
        id: loadingSpy

        target: webViewport
        signalName: "loadingChanged"
    }

    TestCase {
        id: testcaseid

        name: "tst_viewtest"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_viewtest cleanupTestCase")
            wait(1000)
        }

        function test_Test1LoadSimpleBlank() {
            MyScript.dumpTs("test_Test1LoadSimpleBlank start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            webViewport.url = "about:blank"
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100)
            verify(MyScript.wrtWait(function() { return !webViewport.painted }))
            MyScript.dumpTs("test_Test1LoadSimpleBlank end")
        }

        function test_Test2LoadAboutMozillaCheckTitle() {
            MyScript.dumpTs("test_Test2LoadAboutMozillaCheckTitle start")
            loadingSpy.clear()
            webViewport.url = "about:mozilla"
            verify(MyScript.wrtWait(function() { return loadingSpy.count < 2 || webViewport.loading }))
            // Something like The "Book of Mozilla, 6:27", might vary between versions
            verify(webViewport.title.indexOf("The Book of Mozilla") == 0)
            verify(MyScript.wrtWait(function() { return !webViewport.painted }))
            MyScript.dumpTs("test_Test2LoadAboutMozillaCheckTitle end")
        }
    }
}
