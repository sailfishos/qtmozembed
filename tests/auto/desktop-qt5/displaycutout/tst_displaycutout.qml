import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared"

TestWindow {
    id: appWindow

    name: testcaseid.name

    QmlMozView {
        id: webViewport

        property var result
        signal scriptFinished(var result)

        x: 0
        y: 0
        width: 320
        height: 240
        focus: true
        active: true

        onScriptFinished: webViewport.result = result
    }

    SignalSpy {
        id: initSpy
        target: webViewport
        signalName: "viewInitialized"
    }

    SignalSpy {
        id: scriptSpy
        target: webViewport
        signalName: "scriptFinished"
    }

    TestCase {
        id: testcaseid
        name: "tst_displaycutout"
        when: windowShown

        function loadCoverPage() {
            webViewport.loadText(
                        "<!DOCTYPE html><html><head>"
                        + "<meta charset='utf-8'>"
                        + "<meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>"
                        + "</head><body>cutout</body></html>",
                        "text/html")
            tryCompare(webViewport, "loaded", true)
            tryCompare(webViewport, "viewportFit", "cover")
        }

        function initTestCase() {
            if (initSpy.count === 0)
                initSpy.wait()

            compare(initSpy.count, 1)
        }

        function init() {
            webViewport.x = 0
            webViewport.y = 0
            webViewport.width = 320
            webViewport.height = 240
            webViewport.orientation = Qt.PortraitOrientation
            webViewport.safeAreaInsetTop = 0
            webViewport.safeAreaInsetRight = 0
            webViewport.safeAreaInsetBottom = 0
            webViewport.safeAreaInsetLeft = 0
            loadCoverPage()
        }

        function cleanup() {
            scriptSpy.clear()
            webViewport.result = undefined
        }

        function runJavaScript(script) {
            scriptSpy.clear()
            webViewport.result = undefined
            webViewport.runJavaScript(script, function (result) {
                webViewport.scriptFinished(result)
            })
            verify(scriptSpy.wait())
            compare(scriptSpy.count, 1)
            return webViewport.result
        }

        function readSafeAreaInsets() {
            return runJavaScript(
                        "(function() {"
                        + "var root = document.documentElement;"
                        + "root.style.paddingTop = 'env(safe-area-inset-top)';"
                        + "root.style.paddingRight = 'env(safe-area-inset-right)';"
                        + "root.style.paddingBottom = 'env(safe-area-inset-bottom)';"
                        + "root.style.paddingLeft = 'env(safe-area-inset-left)';"
                        + "var style = getComputedStyle(root);"
                        + "return {"
                        + "top: parseInt(style.paddingTop, 10) || 0,"
                        + "right: parseInt(style.paddingRight, 10) || 0,"
                        + "bottom: parseInt(style.paddingBottom, 10) || 0,"
                        + "left: parseInt(style.paddingLeft, 10) || 0"
                        + "};"
                        + "})()")
        }

        function compareSafeAreaInsets(expected) {
            var result = {}
            for (var i = 0; i < 20; ++i) {
                result = readSafeAreaInsets()
                if (result.top === expected.top
                        && result.right === expected.right
                        && result.bottom === expected.bottom
                        && result.left === expected.left) {
                    break
                }
                wait(50)
            }

            compare(result.top, expected.top)
            compare(result.right, expected.right)
            compare(result.bottom, expected.bottom)
            compare(result.left, expected.left)
        }

        function test_viewportFitChanges() {
            compare(webViewport.viewportFit, "cover")

            runJavaScript("document.querySelector(\"meta[name='viewport']\").setAttribute(\"content\", \"width=device-width,initial-scale=1,viewport-fit=contain\")")
            tryCompare(webViewport, "viewportFit", "contain")

            runJavaScript("document.querySelector(\"meta[name='viewport']\").setAttribute(\"content\", \"width=device-width,initial-scale=1\")")
            tryCompare(webViewport, "viewportFit", "auto")

            runJavaScript("document.querySelector(\"meta[name='viewport']\").setAttribute(\"content\", \"width=device-width,initial-scale=1,viewport-fit=cover\")")
            tryCompare(webViewport, "viewportFit", "cover")
        }

        function test_safeAreaInsetsUpdateAcrossOrientations() {
            var cases = [
                { orientation: Qt.PortraitOrientation, top: 11, right: 12, bottom: 13, left: 14 },
                { orientation: Qt.LandscapeOrientation, top: 21, right: 22, bottom: 23, left: 24 },
                { orientation: Qt.InvertedPortraitOrientation, top: 31, right: 32, bottom: 33, left: 34 },
                { orientation: Qt.InvertedLandscapeOrientation, top: 41, right: 42, bottom: 43, left: 44 }
            ]

            for (var i = 0; i < cases.length; ++i) {
                webViewport.orientation = cases[i].orientation
                webViewport.safeAreaInsetTop = cases[i].top
                webViewport.safeAreaInsetRight = cases[i].right
                webViewport.safeAreaInsetBottom = cases[i].bottom
                webViewport.safeAreaInsetLeft = cases[i].left
                compareSafeAreaInsets(cases[i])
            }
        }

        function test_nonZeroScreenPositionChangesLocalInsets() {
            var leftInset = Math.min(400, Math.floor(Screen.width / 3))
            var topInset = Math.min(300, Math.floor(Screen.height / 3))

            webViewport.safeAreaInsetTop = topInset
            webViewport.safeAreaInsetLeft = leftInset
            webViewport.safeAreaInsetRight = 0
            webViewport.safeAreaInsetBottom = 0

            var initial = readSafeAreaInsets()

            webViewport.x = 50
            webViewport.y = 30

            var moved = {}
            for (var i = 0; i < 20; ++i) {
                moved = readSafeAreaInsets()
                if (moved.left !== initial.left || moved.top !== initial.top) {
                    break
                }
                wait(50)
            }

            compare(moved.left, Math.max(initial.left - 50, 0))
            compare(moved.top, Math.max(initial.top - 30, 0))
            compare(moved.right, 0)
            compare(moved.bottom, 0)
        }
    }
}
