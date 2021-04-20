import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

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
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            webViewport.loadFrameScript("chrome://tests/content/testHelper.js")
            appWindow.mozViewInitialized = true
            webViewport.addMessageListener("embed:login")
        }
        onRecvAsyncMessage: {
            print("onRecvAsyncMessage:" + message + ", data:" + data)
            if (message == "embed:login") {
                webViewport.sendAsyncMessage("embedui:login", {
                                                       buttonidx: 1,
                                                       id: data.id
                                                   })
                appWindow.promptReceived = true
            }
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_passwordmgr"
        when: windowShown

        function cleanupTestCase() {
            mozContext.dumpTS("tst_passwordmgr cleanupTestCase")
        }

        function test_TestLoginMgrPage() {
            mozContext.dumpTS("test_TestLoginMgrPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.promptReceived = false
            webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/passwordmgr/subtst_notifications_1.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            mozContext.dumpTS("test_TestLoginMgrPage end");
        }
    }
}
