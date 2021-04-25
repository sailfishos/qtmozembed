import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

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

    TestCase {
        id: testcaseid
        name: "tst_basicmozcontext"
        when: windowShown

        function cleanupTestCase()
        {
            mozContext.dumpTS("tst_basicmozcontext cleanupTestCase")
            // Stop embedding explicitly as we do not have any views.
            mozContext.instance.stopEmbedding()
        }
        function test_context1Init()
        {
            mozContext.dumpTS("test_context1Init start")
            verify(mozContext.instance !== undefined)
            verify(MyScript.wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
            verify(mozContext.instance.isInitialized())
            mozContext.dumpTS("test_context1Init end")
        }
        function test_context3PrefAPI()
        {
            mozContext.dumpTS("test_context3PrefAPI start")
            mozContext.instance.setPref("test.embedlite.pref", "result");
            mozContext.dumpTS("test_context3PrefAPI end")
        }
        function test_context4ObserveAPI()
        {
            mozContext.dumpTS("test_context4ObserveAPI start")
            mozContext.instance.notifyObservers("memory-pressure", null);
            lastObserveMessage = undefined
            mozContext.instance.addObserver("test-observe-message");
            mozContext.instance.notifyObservers("test-observe-message", {msg: "testMessage", val: 1});
            verify(MyScript.wrtWait(function() { return (lastObserveMessage === undefined); }, 10, 500))
            compare(lastObserveMessage.msg, "test-observe-message");
            compare(lastObserveMessage.data.val, 1);
            compare(lastObserveMessage.data.msg, "testMessage");
            mozContext.dumpTS("test_context4ObserveAPI end")
        }
    }
}
