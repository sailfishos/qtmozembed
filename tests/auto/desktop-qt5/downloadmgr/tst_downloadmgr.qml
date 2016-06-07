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
        onRecvObserve: {
            if (message == "embed:download") {
                // print("onRecvObserve: msg:" + message + ", dmsg:" + data.msg);
                if (data.msg == "dl-done") {
                    appWindow.promptReceived = true;
                }
            }
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            webViewport.addMessageListener("embed:filepicker");
            appWindow.mozViewInitialized = true
        }
        onRecvAsyncMessage: {
            // print("onRecvAsyncMessage:" + message + ", data:" + data)
            if (message == "embed:filepicker") {
                webViewport.sendAsyncMessage("filepickerresponse", {
                                                 winid: data.winid,
                                                 accepted: true,
                                                 items: ["/tmp/tt.bin"]
                                             })
            }
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_downloadmgr cleanup")
        }

        function test_TestDownloadMgrPage()
        {
            SharedTests.shared_TestDownloadMgrPage()
        }
    }
}
