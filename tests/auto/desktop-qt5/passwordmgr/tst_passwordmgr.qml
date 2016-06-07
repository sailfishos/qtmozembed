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
    property bool promptReceived

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onOnInitialized: {
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest");
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            webViewport.loadFrameScript("chrome://tests/content/testHelper.js");
            webViewport.loadFrameScript("chrome://embedlite/content/embedhelper.js");
            appWindow.mozViewInitialized = true
            webViewport.addMessageListener("embed:login");
        }
        onRecvAsyncMessage: {
            print("onRecvAsyncMessage:" + message + ", data:" + data)
            if (message == "embed:login") {
                webViewport.sendAsyncMessage("embedui:login", {
                                                       buttonidx: 1,
                                                       id: data.id
                                                   })
                appWindow.promptReceived = true;
            }
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_passwordmgr cleanup")
        }

        function test_TestLoginMgrPage()
        {
            SharedTests.shared_TestLoginMgrPage()
        }
    }
}
