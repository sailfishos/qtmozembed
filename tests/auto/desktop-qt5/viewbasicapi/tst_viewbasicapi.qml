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

    QmlMozContext {
        id: mozContext
    }

    TestCase {
        id: testcaseid
        name: "tst_viewbasicapi"
        when: windowShown

        function cleanupTestCase() {
            mozContext.dumpTS("tst_viewbasicapi cleanup")
        }

        function test_2viewInit() {
            mozContext.dumpTS("test_2viewInit start")
            testcaseid.verify(mozContext.instance.isInitialized())
            MyScript.createSpriteObjects()
            while (mozView == null) {
                testcaseid.wait(500)
            }
            mozContext.dumpTS("test_2viewInit start1")
            testcaseid.verify(MyScript.waitMozView())
            testcaseid.verify(mozView.uniqueId > 0)
            testcaseid.verify(mozView.child)
            mozContext.dumpTS("test_2viewInit end")
        }
    }
}
