import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property bool promptReceived

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
            MyScript.dumpTs("tst_passwordmgr cleanupTestCase")
        }

        function test_TestLoginMgrPage() {
            skip("Not working properly, please see JB#56715")
            MyScript.dumpTs("test_TestLoginMgrPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.promptReceived = false
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/passwordmgr/subtst_notifications_1.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            MyScript.dumpTs("test_TestLoginMgrPage end");
        }
    }
}
