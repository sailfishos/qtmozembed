import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    name: testcaseid.name

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

        function test_3viewInsets() {
            mozView.marginTop = 11
            mozView.marginRight = 12
            mozView.marginBottom = 13
            mozView.marginLeft = 14
            compare(mozView.marginTop, 11)
            compare(mozView.marginRight, 12)
            compare(mozView.marginBottom, 13)
            compare(mozView.marginLeft, 14)

            mozView.safeAreaInsetTop = 21
            mozView.safeAreaInsetRight = 22
            mozView.safeAreaInsetBottom = 23
            mozView.safeAreaInsetLeft = 24
            compare(mozView.safeAreaInsetTop, 21)
            compare(mozView.safeAreaInsetRight, 22)
            compare(mozView.safeAreaInsetBottom, 23)
            compare(mozView.safeAreaInsetLeft, 24)

            mozView.marginTop = 31
            compare(mozView.marginTop, 31)
            compare(mozView.marginRight, 12)
            compare(mozView.marginBottom, 13)
            compare(mozView.marginLeft, 14)
        }
    }
}
