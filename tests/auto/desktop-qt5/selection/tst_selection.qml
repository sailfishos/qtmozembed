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
    property string selectedContent

    Connections {
        target: QmlMozContext
        onRecvObserve: {
            if (message == "clipboard:setdata") {
                print("onRecvObserve: msg:", message, ", data:", data.data)
                appWindow.selectedContent = data.data || ""
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
            appWindow.mozViewInitialized = true
            webViewport.addMessageListener("Content:ContextMenu")
            webViewport.addMessageListener("Content:SelectionRange")
            webViewport.addMessageListener("Content:SelectionCopied")
        }
        onRecvAsyncMessage: {
            print("onRecvAsyncMessage:" + message + ", data:" + data)
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_selection"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_selection cleanupTestCase")
        }

        function test_SelectionInit()
        {
            MyScript.dumpTs("test_SelectionInit start")
            testcaseid.verify(MyScript.waitMozContext())
            QmlMozContext.addObserver("clipboard:setdata");
            testcaseid.verify(MyScript.waitMozView())
            webViewport.url = "data:text/html,hello test selection";
            testcaseid.verify(MyScript.waitLoadFinished(webViewport))
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            webViewport.sendAsyncMessage("Browser:SelectionStart", {
                                                xPos: 56,
                                                yPos: 16
                                              })
            webViewport.sendAsyncMessage("Browser:SelectionMoveStart", {
                                                change: "start"
                                              })
            webViewport.sendAsyncMessage("Browser:SelectionCopy", {
                                                xPos: 56,
                                                yPos: 16
                                              })
            testcaseid.verify(MyScript.wrtWait(function() { return (appWindow.selectedContent == ""); }))
            testcaseid.compare(appWindow.selectedContent, "test");
            MyScript.dumpTs("test_SelectionInit end")
        }
    }
}
