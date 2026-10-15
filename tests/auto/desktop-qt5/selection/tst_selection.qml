import QtTest 1.0
import QtQuick 2.6
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property int selectionUpdates
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
            if (message === "Content:SelectionRange")
                ++appWindow.selectionUpdates
        }
    }

    SignalSpy {
        id: loadSpy

        target: webViewport
        signalName: "loadingChanged"
    }

    TestCase {
        id: testcaseid

        name: "tst_selection"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_selection cleanupTestCase")
        }

        function test_SelectionInit_data() {
            var rows = []
            for (var i = 0; i < 10; ++i)
                rows.push({tag: "navigation-" + i})
            return rows
        }

        function test_SelectionInit(data)
        {
            MyScript.dumpTs("test_SelectionInit start")
            testcaseid.verify(MyScript.waitMozContext())
            QmlMozContext.addObserver("clipboard:setdata");
            testcaseid.verify(MyScript.waitMozView())
            appWindow.selectedContent = ""
            appWindow.selectionUpdates = 0
            loadSpy.clear()
            webViewport.url = "data:text/html,<!DOCTYPE html><title>" + data.tag
                    + "</title><body>hello test selection</body>"
            testcaseid.verify(MyScript.wrtWait(function() { return loadSpy.count < 2 || webViewport.loading }))
            testcaseid.tryCompare(webViewport, "title", data.tag)
            testcaseid.compare(webViewport.loadProgress, 100);
            testcaseid.verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            webViewport.sendAsyncMessage("Browser:SelectionStart", {xPos: 0, yPos: 0})
            testcaseid.verify(MyScript.wrtWait(function() { return appWindow.selectionUpdates === 0 }, 100, 50))
            var updates = appWindow.selectionUpdates
            webViewport.sendAsyncMessage("Browser:SelectionSelectAll", {})
            testcaseid.verify(MyScript.wrtWait(function() { return appWindow.selectionUpdates === updates }, 100, 50))
            webViewport.sendAsyncMessage("Browser:SelectionCopy", {xPos: 10, yPos: 10})
            testcaseid.verify(MyScript.wrtWait(function() { return (appWindow.selectedContent == ""); }))
            testcaseid.compare(appWindow.selectedContent, "hello test selection");
            MyScript.dumpTs("test_SelectionInit end")
        }
    }
}
