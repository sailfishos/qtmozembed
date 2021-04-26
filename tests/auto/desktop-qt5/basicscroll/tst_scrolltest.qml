import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800
    focus: true

    property bool mozViewInitialized
    property int scrollX
    property int scrollY

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent

        onViewInitialized: appWindow.mozViewInitialized = true
        onViewAreaChanged: {
            print("onViewAreaChanged: ", webViewport.scrollableOffset.x, webViewport.scrollableOffset.y)
            var offset = webViewport.scrollableOffset
            appWindow.scrollX = offset.x
            appWindow.scrollY = offset.y
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_scrolltest"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_scrolltest cleanupTestCase")
        }

        function test_TestScrollPaintOperations() {
            MyScript.dumpTs("test_TestScrollPaintOperations start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body bgcolor=red leftmargin=0 topmargin=0 marginwidth=0 marginheight=0><input style='position:absolute; left:0px; top:1200px;'>";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            while (appWindow.scrollY === 0) {
                MyScript.scrollBy(100, 301, 0, -200, 100, false);
                wait(100);
            }
            verify(appWindow.scrollX === 0)
            MyScript.dumpTs("test_TestScrollPaintOperations end");
        }
    }
}
