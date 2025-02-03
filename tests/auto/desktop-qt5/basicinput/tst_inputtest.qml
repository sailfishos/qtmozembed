import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property string inputContent
    property int inputState: -1
    property bool changed
    property int focusChange: -1
    property int cause: -1
    property string inputType

    function isState(state, focus, cause) {
        return appWindow.changed === true && appWindow.inputState === state && appWindow.focusChange === focus
                && appWindow.cause === cause
    }

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
    }

    QmlMozView {
        id: webViewport

        clip: false
        visible: true
        focus: true
        active: true

        anchors.fill: parent
        onViewInitialized: {
            appWindow.mozViewInitialized = true
        }
        onHandleSingleTap: {
            print("HandleSingleTap: [",point.x,",",point.y,"]")
        }
        onRecvAsyncMessage: {
            // print("onRecvAsyncMessage:" + message + ", data:" + data)
            switch (message) {
            case "testembed:elementpropvalue": {
                // print("testembed:elementpropvalue value:" + data.value)
                appWindow.inputContent = data.value
                break
            }
            default:
                break
            }
        }
        onImeNotification: {
            print("onImeNotification: state:" + state + ", open:" + open + ", cause:" + cause + ", focChange:"
                  + focusChange + ", type:" + type)
            appWindow.changed = true
            appWindow.inputState = state
            appWindow.cause = cause
            appWindow.focusChange = focusChange
            appWindow.inputType = type
        }
        Component.onCompleted: {
            webViewport.loadFrameScript("chrome://tests/content/testHelper.js")
            webViewport.addMessageListener("testembed:elementpropvalue")
        }
    }

    TestCase {
        id: testcaseid

        name: "tst_inputtest"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_inputtest cleanupTestCase")
        }

        function test_Test1LoadInputPage() {
            MyScript.dumpTs("test_Test1LoadInputPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1' charset='utf-8'></head><body><input id=myelem value=''>"
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100)
            verify(MyScript.wrtWait(function() { return !webViewport.painted }))
            mouseClick(webViewport, 10, 10)
            verify(MyScript.wrtWait(function() { return !appWindow.isState(1, 0, 4) }))
            appWindow.inputState = false
            keyClick(Qt.Key_K)
            keyClick(Qt.Key_O)
            keyClick(Qt.Key_R)
            keyClick(Qt.Key_P)
            webViewport.sendAsyncMessage("embedtest:getelementprop", {
                                                name: "myelem",
                                                property: "value"
                                               })
            verify(MyScript.wrtWait(function() { return appWindow.inputContent == "" }))
            compare(appWindow.inputContent, "korp")
            MyScript.dumpTs("test_Test1LoadInputPage end")
        }

        function test_Test1LoadInputURLPage() {
            MyScript.dumpTs("test_Test1LoadInputURLPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.inputContent = ""
            appWindow.inputType = ""
            webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1' charset='utf-8'></head><body><input type=number id=myelem value=''>"
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100)
            verify(MyScript.wrtWait(function() { return !webViewport.painted }))
            mouseClick(webViewport, 10, 10)
            verify(MyScript.wrtWait(function() { return !appWindow.isState(1, 0, 4) }))
            appWindow.inputState = false
            keyClick(Qt.Key_1)
            keyClick(Qt.Key_2)
            keyClick(Qt.Key_3)
            keyClick(Qt.Key_4)
            webViewport.sendAsyncMessage("embedtest:getelementprop", {
                                                name: "myelem",
                                                property: "value"
                                               })
            verify(MyScript.wrtWait(function() { return appWindow.inputContent == "" }))
            verify(MyScript.wrtWait(function() { return appWindow.inputType == "" }))
            compare(appWindow.inputContent, "1234")
            MyScript.dumpTs("test_Test1LoadInputURLPage end")
        }
    }

    Component.onCompleted: {
        QMozEngineSettings.setPreference("embedlite.azpc.handle.singletap", false)
        QMozEngineSettings.setPreference("embedlite.azpc.json.singletap", true)
    }
}
