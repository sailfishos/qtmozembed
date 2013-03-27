import QtQuickTest 1.0
import QtQuick 1.0
import Sailfish.Silica 1.0
import QtMozilla 1.0
import "componentCreation.js" as MyScript

ApplicationWindow {
    id: appWindow

    property string currentPageName: pageStack.currentPage != null
            ? pageStack.currentPage.objectName
            : ""

    property bool mozViewInitialized : false
    property variant mozView : null
    property variant lastObserveMessage : null

    QmlMozContext {
        id: mozContext
        autoinit: false
    }
    Connections {
        target: mozContext.child
        onOnInitialized: {
            // Gecko does not switch to SW mode if gl context failed to init
            // and qmlmoztestrunner does not build in GL mode
            // Let's put it here for now in SW mode always
            mozContext.child.setIsAccelerated(false);
        }
        onRecvObserve: {
            lastObserveMessage = { msg: message, data: data }
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown

        function cleanup() {
            // Return to the start page
            appWindow.pageStack.pop(null, PageStackAction.Immediate)
        }

        function test_context1Init()
        {
            verify(mozContext.child !== undefined)
            while (mozContext.child.initialized() === false) {
                wait(500)
            }
            verify(mozContext.child.initialized())
        }
        function test_context2AcceleratedAPI()
        {
            mozContext.child.setIsAccelerated(true);
            verify(mozContext.child.isAccelerated() === true)
            mozContext.child.setIsAccelerated(false);
            verify(mozContext.child.isAccelerated() === false)
        }
        function test_context3PrefAPI()
        {
            mozContext.child.setPref("test.embedlite.pref", "result");
        }
        function test_context4ObserveAPI()
        {
            mozContext.child.sendObserve("memory-pressure", null);
            mozContext.child.addObserver("test-observe-message");
            mozContext.child.sendObserve("test-observe-message", {msg: "testMessage", val: 1});
            wait(50)
            compare(lastObserveMessage.msg, "test-observe-message");
            compare(lastObserveMessage.data.val, 1);
            compare(lastObserveMessage.data.msg, "testMessage");
        }
    }
}
