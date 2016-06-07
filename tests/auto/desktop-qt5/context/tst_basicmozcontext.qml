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
    property var mozView
    property var lastObserveMessage

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onRecvObserve: {
            lastObserveMessage = { msg: message, data: data }
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup()
        {
            mozContext.dumpTS("tst_basicmozcontext cleanup")
        }
        function test_context1Init()
        {
            SharedTests.shared_context1Init()
        }
        function test_context3PrefAPI()
        {
            SharedTests.shared_context3PrefAPI()
        }
        function test_context4ObserveAPI()
        {
            SharedTests.shared_context4ObserveAPI()
        }
    }
}
