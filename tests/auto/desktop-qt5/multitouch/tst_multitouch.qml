import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared/sharedTests.js" as SharedTests

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
            SharedTests.shared_Test1MultiTouchPage()
        }
    }
}
