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
    property string selectedContent

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onRecvObserve: {
            print("onRecvObserve: msg:", message, ", data:", data.data)
            appWindow.selectedContent = data.data
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
            webViewport.addMessageListeners([ "Content:ContextMenu", "Content:SelectionRange", "Content:SelectionCopied" ])
        }
        onRecvAsyncMessage: {
            print("onRecvAsyncMessage:" + message + ", data:" + data)
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_inputtest cleanup")
        }

        function test_SelectionInit()
        {
            mozContext.dumpTS("test_SelectionInit start")
            testcaseid.verify(MyScript.waitMozContext())
            mozContext.instance.addObserver("clipboard:setdata");
            testcaseid.verify(MyScript.waitMozView())
            webViewport.url = "data:text/html,hello test selection";
            testcaseid.verify(MyScript.waitLoadFinished(webViewport))
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(SharedTests.wrtWait(function() { return (!webViewport.painted); }))
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
            testcaseid.verify(SharedTests.wrtWait(function() { return (appWindow.selectedContent == ""); }))
            testcaseid.compare(appWindow.selectedContent, "test");
            mozContext.dumpTS("test_SelectionInit end")
        }
    }
}
