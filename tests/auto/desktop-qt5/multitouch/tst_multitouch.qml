import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var testResult: ""

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onOnInitialized: {
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        Connections {
            target: webViewport.child
            onViewInitialized: {
                webViewport.child.loadFrameScript("chrome://tests/content/testHelper.js")
                appWindow.mozViewInitialized = true
                webViewport.child.addMessageListener("testembed:elementinnervalue")
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

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_multitouch cleanup")
        }

        function test_Test1MultiTouchPage()
        {
            mozContext.dumpTS("test_Test1MultiTouchPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/multitouch/touch.html";
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
            mozContext.dumpTS("test_Test1MultiTouchPage end");
        }
    }
}
