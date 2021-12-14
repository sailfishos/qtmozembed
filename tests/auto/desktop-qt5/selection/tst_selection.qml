import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property string selectedContent

    name: testcaseid.name

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
            webViewport.sendAsyncMessage("Browser:SelectionStart", {xPos: 0, yPos: 0})
            webViewport.sendAsyncMessage("Browser:SelectionSelectAll", {})
            webViewport.sendAsyncMessage("Browser:SelectionCopy", {xPos: 10, yPos: 10})
            testcaseid.verify(MyScript.wrtWait(function() { return (appWindow.selectedContent == ""); }))
            testcaseid.compare(appWindow.selectedContent, "hello test selection");
            MyScript.dumpTs("test_SelectionInit end")
        }
    }
}
