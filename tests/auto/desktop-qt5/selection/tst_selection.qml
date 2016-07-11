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
            webViewport.loadFrameScript("chrome://embedlite/content/embedhelper.js")
            webViewport.loadFrameScript("chrome://embedlite/content/SelectHelper.js")
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
            SharedTests.shared_SelectionInit()
        }
    }
}
