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
    property bool promptReceived
    property var testResult
    property int testCaseNum

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onOnInitialized: {
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            webViewport.loadFrameScript("chrome://tests/content/testHelper.js")
            appWindow.mozViewInitialized = true
            webViewport.addMessageListener("testembed:elementinnervalue")
            webViewport.addMessageListener("embed:prompt")
        }
        onRecvAsyncMessage: {
            // print("onRecvAsyncMessage:" + message + ", data:" + data)
            if (message == "embed:prompt") {
                testcaseid.compare(data.defaultValue, "Your name")
                testcaseid.compare(data.text, "Please enter your name:")
                var responsePrompt = null
                switch(appWindow.testCaseNum) {
                case 0:
                    responsePrompt = "expectedPromptResult"
                    break
                case 1:
                    responsePrompt = "unexpectedPromptResult"
                    break
                }
                if (responsePrompt) {
                    webViewport.sendAsyncMessage("promptresponse", {
                                                     winid: data.winid,
                                                     checkval: true,
                                                     accepted: true,
                                                     promptvalue: responsePrompt
                                                 })
                }
                appWindow.promptReceived = true
            } else if (message == "testembed:elementinnervalue") {
                appWindow.testResult = data.value
            }
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_multitouch cleanup")
        }

        function test_TestPromptPage()
        {
            SharedTests.shared_TestPromptPage()
        }

        function test_TestPromptWithBadResponse()
        {
            SharedTests.shared_TestPromptWithBadResponse()
        }

        function test_TestPromptWithoutResponse()
        {
            SharedTests.shared_TestPromptWithoutResponse()
        }
    }
}
