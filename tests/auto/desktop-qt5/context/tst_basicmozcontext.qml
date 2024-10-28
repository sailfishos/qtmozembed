import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property var lastObserveMessage

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onRecvObserve: {
            lastObserveMessage = { msg: message, data: data }
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_basicmozcontext"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_basicmozcontext cleanupTestCase")
        }

        function test_context1Init() {
            MyScript.dumpTs("test_context1Init start")
            verify(MyScript.wrtWait(function() { return (QmlMozContext.isInitialized() === false); }, 100, 500))
            MyScript.createSpriteObjects()
            verify(QmlMozContext.isInitialized())
            MyScript.dumpTs("test_context1Init end")
        }

        function test_context3PrefAPI() {
            MyScript.dumpTs("test_context3PrefAPI start")
            QMozEngineSettings.setPreference("test.embedlite.pref", "result");
            MyScript.dumpTs("test_context3PrefAPI end")
        }

        function test_context4ObserveAPI() {
            MyScript.dumpTs("test_context4ObserveAPI start")
            QmlMozContext.notifyObservers("memory-pressure", null)
            lastObserveMessage = undefined
            QmlMozContext.addObserver("test-observe-message")
            QmlMozContext.notifyObservers("test-observe-message", {msg: "testMessage", val: 1})
            verify(MyScript.wrtWait(function() { return lastObserveMessage === undefined }, 10, 500))
            compare(lastObserveMessage.msg, "test-observe-message")
            compare(lastObserveMessage.data.val, 1)
            compare(lastObserveMessage.data.msg, "testMessage")
            MyScript.dumpTs("test_context4ObserveAPI end")
        }
    }
}
