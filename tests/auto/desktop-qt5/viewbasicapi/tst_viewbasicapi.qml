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

    TestCase {
        id: testcaseid
        name: "tst_viewbasicapi"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_viewbasicapi cleanup")
        }

        function test_2viewInit() {
            MyScript.dumpTs("test_2viewInit start")
            testcaseid.verify(QmlMozContext.isInitialized())
            MyScript.createSpriteObjects()
            while (mozView == null) {
                testcaseid.wait(500)
            }
            MyScript.dumpTs("test_2viewInit start1")
            testcaseid.verify(MyScript.waitMozView())
            testcaseid.verify(mozView)
            testcaseid.verify(mozView.uniqueId > 0)
            MyScript.dumpTs("test_2viewInit end")
        }
    }
}
