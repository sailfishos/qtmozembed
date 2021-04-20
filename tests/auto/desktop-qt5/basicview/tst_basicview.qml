import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var mozView
    property int createParentID

    QmlMozContext {
        id: mozContext
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_basicview cleanup")
        }

        function test_1contextPrepareViewContext()
        {
            mozContext.dumpTS("test_1contextPrepareViewContext start")
            verify(mozContext.instance !== undefined)
            verify(MyScript.wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
            verify(mozContext.instance.isInitialized())
            mozContext.dumpTS("test_1contextPrepareViewContext end")
        }
        function test_2viewInit()
        {
            mozContext.dumpTS("test_2viewInit start")
            verify(MyScript.wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
            verify(mozContext.instance.isInitialized())
            appWindow.createParentID = 0;
            MyScript.createSpriteObjects();
            verify(MyScript.wrtWait(function() { return (mozView === undefined); }))
            verify(MyScript.wrtWait(function() { return (mozViewInitialized !== true); }))
            verify(mozView !== undefined)
            mozContext.dumpTS("test_2viewInit end")
        }
        function test_3viewLoadURL()
        {
            mozContext.dumpTS("test_3viewLoadURL start")
            verify(mozView !== undefined)
            mozView.url = "about:mozilla";
            verify(MyScript.waitLoadFinished(mozView))
            verify(MyScript.wrtWait(function() { return (mozView.url === "about:mozilla"); }))
            verify(MyScript.wrtWait(function() { return (!mozView.painted); }))
            mozContext.dumpTS("test_3viewLoadURL end")
        }
    }
}
