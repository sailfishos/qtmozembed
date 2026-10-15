import QtTest 1.0
import QtQuick 2.6
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    name: testcaseid.name

    TestCase {
        id: testcaseid

        name: "tst_basicview"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_basicview cleanupTestCase")
            wait(1000)
        }

        function test_1contextPrepareViewContext() {
            MyScript.dumpTs("test_1contextPrepareViewContext start")
            verify(MyScript.wrtWait(function() { return QmlMozContext.isInitialized() === false }, 100, 500))
            verify(QmlMozContext.isInitialized())
            MyScript.dumpTs("test_1contextPrepareViewContext end")
        }

        function test_2viewInit() {
            MyScript.dumpTs("test_2viewInit start")
            verify(MyScript.wrtWait(function() { return QmlMozContext.isInitialized() === false }, 100, 500))
            verify(QmlMozContext.isInitialized())
            appWindow.createParentID = 0
            MyScript.createSpriteObjects()
            verify(MyScript.wrtWait(function() { return mozView === undefined }))
            verify(MyScript.wrtWait(function() { return mozViewInitialized !== true }))
            verify(mozView !== undefined)
            verify(MyScript.wrtWait(function() { return mozView.selectedTabIndex < 0 }))
            compare(mozView.selectedTabIndex, 0)
            MyScript.dumpTs("test_2viewInit end")
        }

        function test_3viewLoadURL() {
            MyScript.dumpTs("test_3viewLoadURL start")
            verify(mozView !== undefined)
            mozView.url = "about:mozilla"
            verify(MyScript.waitLoadFinished(mozView))
            verify(MyScript.wrtWait(function() { return mozView.url === "about:mozilla" }))
            verify(MyScript.wrtWait(function() { return !mozView.painted }))
            MyScript.dumpTs("test_3viewLoadURL end")
        }
        function test_4delayedStylesheet() {
            var base = TestHelper.getenv("QTTESTSROOT") + "/auto/desktop-qt5/basicview/"
            mozView.url = TestHelper.serveStyledFile(base + "delayed-style.html",
                                                    base + "delayed-style.css", 750)
            tryCompare(mozView, "title", "Delayed stylesheet", 10000)
            tryCompare(mozView, "loading", false, 10000)
            tryCompare(mozView, "painted", true, 10000)
            var firstFrame
            for (var attempt = 0; attempt < 100 && !firstFrame; ++attempt) {
                mozView.runJavaScript("return window.firstFrame", function(result) {
                    firstFrame = result
                })
                wait(50)
            }
            verify(firstFrame !== undefined && firstFrame !== null)
            compare(firstFrame.width, "137px")
            compare(firstFrame.styled, true)
        }
    }
}
