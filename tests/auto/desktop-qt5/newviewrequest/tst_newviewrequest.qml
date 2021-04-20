import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import qtmozembed.tests 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var mozView
    property var oldMozView
    property int createParentID

    QmlMozContext {
        id: mozContext
    }

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

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_newviewrequest cleanup")
        }

        function test_1newcontextPrepareViewContext()
        {
            mozContext.dumpTS("test_1newcontextPrepareViewContext start")
            verify(mozContext.instance !== undefined)
            verify(MyScript.wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
            verify(mozContext.instance.isInitialized())
            mozContext.dumpTS("test_1newcontextPrepareViewContext end")
        }
        function test_2newviewInit()
        {
            mozContext.dumpTS("test_2newviewInit start")
            verify(MyScript.wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
            verify(mozContext.instance.isInitialized())
            MyScript.createSpriteObjects();
            verify(MyScript.wrtWait(function() { return (mozView === null); }, 10, 500))
            verify(MyScript.wrtWait(function() { return (mozViewInitialized !== true); }, 10, 500))
            verify(mozView !== undefined)
            mozContext.dumpTS("test_2newviewInit end")
        }
        function test_viewTestNewWindowAPI()
        {
            mozContext.dumpTS("test_viewTestNewWindowAPI start")
            verify(MyScript.wrtWait(function() { return (mozView === undefined); }, 100, 500))
            verify(mozView !== undefined)
            mozView.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/newviewrequest/newwin.html";
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
            mozContext.dumpTS("test_viewTestNewWindowAPI end")
        }
    }
}
