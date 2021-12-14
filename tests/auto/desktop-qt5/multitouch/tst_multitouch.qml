import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property var testResult: ""

    name: testcaseid.name

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
        Connections {
            target: webViewport
            onViewInitialized: {
                webViewport.loadFrameScript("chrome://tests/content/testHelper.js")
                appWindow.mozViewInitialized = true
                webViewport.addMessageListener("testembed:elementinnervalue")
            }
            onHandleSingleTap: {
                print("HandleSingleTap: [",point.x,",",point.y,"]")
            }
            onRecvAsyncMessage: {
                // print("onRecvAsyncMessage:" + message + ", data:" + data)
                switch (message) {
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
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/multitouch/touch.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            var params = [Qt.point(50,50), Qt.point(51,51), Qt.point(52,52)];
            webViewport.synthTouchBegin(params);
            params = [Qt.point(51,51), Qt.point(52,52), Qt.point(53,53)];
            webViewport.synthTouchMove(params);
            params = [Qt.point(52,52), Qt.point(53,53), Qt.point(54,54)];
            webViewport.synthTouchEnd(params);
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                                name: "result" })
            verify(MyScript.wrtWait(function() { return (appWindow.testResult == ""); }))
            compare(appWindow.testResult, "ok");
            MyScript.dumpTs("test_Test1MultiTouchPage end");
        }
    }
}
