import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var mozView
    property var oldMozView
    property int createParentID

    WebViewCreator {
        onNewWindowRequested: {
            print("New Window Requested: url: ", mozView.url, ", parentID:", parentId)
            appWindow.oldMozView = appWindow.mozView
            appWindow.mozView = null
            appWindow.createParentID = parentId
            MyScript.createSpriteObjects()
            while (appWindow.mozView === null) {
                testcaseid.wait()
            }
            testcaseid.verify(mozView.uniqueId > 0)
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_newviewrequest"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_newviewrequest cleanupTestCase")
        }

        function test_1newcontextPrepareViewContext() {
            MyScript.dumpTs("test_1newcontextPrepareViewContext start")
            verify(MyScript.wrtWait(function() { return (QmlMozContext.isInitialized() === false); }, 100, 500))
            verify(QmlMozContext.isInitialized())
            MyScript.dumpTs("test_1newcontextPrepareViewContext end")
        }

        function test_2newviewInit() {
            MyScript.dumpTs("test_2newviewInit start")
            verify(MyScript.wrtWait(function() { return (QmlMozContext.isInitialized() === false); }, 100, 500))
            verify(QmlMozContext.isInitialized())
            MyScript.createSpriteObjects();
            verify(MyScript.wrtWait(function() { return (mozView === null); }, 10, 500))
            verify(MyScript.wrtWait(function() { return (mozViewInitialized !== true); }, 10, 500))
            verify(mozView !== undefined)
            MyScript.dumpTs("test_2newviewInit end")
        }

        function test_viewTestNewWindowAPI() {
            MyScript.dumpTs("test_viewTestNewWindowAPI start")
            verify(MyScript.wrtWait(function() { return (mozView === undefined); }, 100, 500))
            verify(mozView !== undefined)
            mozView.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/newviewrequest/newwin.html";
            verify(MyScript.waitLoadFinished(mozView))
            compare(mozView.title, "NewWinExample")
            verify(MyScript.wrtWait(function() { return (!mozView.painted); }))
            mozViewInitialized = false;
            mouseClick(mozView, 10, 10)
            verify(MyScript.wrtWait(function() { return (!mozView || !oldMozView); }))
            verify(MyScript.wrtWait(function() { return (mozViewInitialized !== true); }))
            verify(mozView !== undefined)
            verify(MyScript.waitLoadFinished(mozView))
            verify(MyScript.wrtWait(function() { return (!mozView.painted); }))
            compare(mozView.url, "about:license")
            MyScript.dumpTs("test_viewTestNewWindowAPI end")
        }
    }
}
