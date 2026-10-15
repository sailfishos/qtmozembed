import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property bool apzReady
    property var testResult: ""

    name: testcaseid.name

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        Connections {
            target: webViewport
            onViewInitialized: {
                webViewport.loadFrameScript("chrome://tests/content/testHelper.js")
                appWindow.mozViewInitialized = true
                webViewport.addMessageListener("testembed:elementinnervalue")
                webViewport.addMessageListener("testembed:apzready")
            }
            onHandleSingleTap: {
                print("HandleSingleTap: [",point.x,",",point.y,"]")
            }
            onRecvAsyncMessage: {
                // print("onRecvAsyncMessage:" + message + ", data:" + data)
                switch (message) {
                case "testembed:apzready":
                    appWindow.apzReady = data.value
                    break;
                case "testembed:elementinnervalue": {
                    // print("testembed:elementpropvalue value:" + data.value)
                    appWindow.testResult = data.value
                    break
                }
                default:
                    break
                }

            }
        }
    }

    SignalSpy {
        id: loadSpy

        target: webViewport
        signalName: "loadingChanged"
    }

    TestCase {
        id: testcaseid
        name: "tst_multitouch"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_multitouch cleanup")
        }

        function test_Test1MultiTouchPage() {
            MyScript.dumpTs("test_Test1MultiTouchPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            // Wait for this navigation, not the preceding about:blank load.
            loadSpy.clear()
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/multitouch/touch.html";
            verify(MyScript.wrtWait(function() { return loadSpy.count < 2 || webViewport.loading }))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            webViewport.sendAsyncMessage("embedtest:flushapz", {})
            tryCompare(appWindow, "apzReady", true, 5000)
            var params = [Qt.point(50,50), Qt.point(51,51), Qt.point(52,52)];
            webViewport.synthTouchBegin(params);
            params = [Qt.point(51,51), Qt.point(52,52), Qt.point(53,53)];
            webViewport.synthTouchMove(params);
            params = [Qt.point(52,52), Qt.point(53,53), Qt.point(54,54)];
            webViewport.synthTouchEnd(params);
            // Touch delivery and the frame-script reply are asynchronous.
            for (var attempt = 0; attempt < 100 && appWindow.testResult === ""; ++attempt) {
                webViewport.sendAsyncMessage("embedtest:getelementinner", { name: "result" })
                wait(50)
            }
            compare(appWindow.testResult, "ok");
            MyScript.dumpTs("test_Test1MultiTouchPage end");
        }
    }
}
