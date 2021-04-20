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

    Component.onCompleted: {
        mozContext.instance.setPref("browser.download.folderList", 2); // 0 - Desktop, 1 - Downloads, 2 - Custom
        mozContext.instance.setPref("browser.download.useDownloadDir", false); // Invoke filepicker instead of immediate download to ~/Downloads
        mozContext.instance.setPref("browser.download.manager.retention", 2);
        mozContext.instance.setPref("browser.helperApps.deleteTempFileOnExit", false);
    }

    Connections {
        target: mozContext.instance
        onOnInitialized: {
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
        onRecvObserve: {
            if (message == "embed:download") {
                // print("onRecvObserve: msg:" + message + ", dmsg:" + data.msg)
                if (data.msg == "dl-done") {
                    appWindow.promptReceived = true
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
            webViewport.addMessageListener("embed:downloadpicker")
            appWindow.mozViewInitialized = true
        }
        onRecvAsyncMessage: {
            if (message == "embed:downloadpicker") {
                mozContext.instance.notifyObservers("embedui:downloadpicker", {
                                                 downloadDirectory: "/tmp/",
                                                 defaultFileName: data.defaultFileName,                                             })
                                                 suggestedFileExtension: data.suggestedFileExtension
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
            mozContext.dumpTS("test_TestLoginMgrPage start")
            testcaseid.verify(MyScript.waitMozContext())
            mozContext.instance.addObserver("embed:download");
            testcaseid.verify(MyScript.waitMozView())
            appWindow.promptReceived = false
            webViewport.url = "about:mozilla";
            testcaseid.verify(MyScript.waitLoadFinished(webViewport))
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/downloadmgr/tt.bin";
            testcaseid.verify(MyScript.waitLoadFinished(webViewport))
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            mozContext.dumpTS("test_TestDownloadMgrPage end");
        }
    }
}
