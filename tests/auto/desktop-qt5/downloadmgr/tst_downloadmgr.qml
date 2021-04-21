import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property bool promptReceived

    Component.onCompleted: {
        QMozEngineSettings.setPreference("browser.download.folderList", 2); // 0 - Desktop, 1 - Downloads, 2 - Custom
        QMozEngineSettings.setPreference("browser.download.useDownloadDir", false); // Invoke filepicker instead of immediate download to ~/Downloads
        QMozEngineSettings.setPreference("browser.download.manager.retention", 2);
        QMozEngineSettings.setPreference("browser.helperApps.deleteTempFileOnExit", false);
    }

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
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
                QmlMozContext.notifyObservers("embedui:downloadpicker", {
                                                 downloadDirectory: "/tmp/",
                                                 defaultFileName: data.defaultFileName,                                             })
                                                 suggestedFileExtension: data.suggestedFileExtension
            }
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_downloadmgr"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_downloadmgr cleanupTestCase")
        }

        function test_TestDownloadMgrPage() {
            MyScript.dumpTs("test_TestDownloadMgrPage start")
            testcaseid.verify(MyScript.waitMozContext())
            QmlMozContext.addObserver("embed:download");
            testcaseid.verify(MyScript.waitMozView())
            appWindow.promptReceived = false
            webViewport.url = "about:mozilla";
            testcaseid.verify(MyScript.waitLoadFinished(webViewport))
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/downloadmgr/tt.bin";
            testcaseid.verify(MyScript.waitLoadFinished(webViewport))
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            MyScript.dumpTs("test_TestDownloadMgrPage end");
        }
    }
}
