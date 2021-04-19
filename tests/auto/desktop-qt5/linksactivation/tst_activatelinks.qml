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

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onRecvObserve: {
            print("onRecvObserve: msg:", message, ", data:", data.data)
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            webViewport.useQmlMouse = true
            appWindow.mozViewInitialized = true
        }
    }

    MouseArea {
        id: viewportMouse
        anchors.fill: parent
        onPressed: webViewport.recvMousePress(mouseX, mouseY)
        onReleased: webViewport.recvMouseRelease(mouseX, mouseY)
        onPositionChanged: webViewport.recvMouseMove(mouseX, mouseY)
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_inputtest cleanup")
        }

        function test_ActiveHyperLink()
        {
            SharedTests.shared_ActiveHyperLink()
            webViewport.useQmlMouse = false
        }
    }
}
