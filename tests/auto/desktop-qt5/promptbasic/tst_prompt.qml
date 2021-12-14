import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property bool promptReceived
    property var testResult
    property int testCaseNum
    property var responseMessages: []

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
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
//                testcaseid.compare(data.defaultValue, "Your name") // No such thing..?
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

                responseMessages[appWindow.testCaseNum] = {
                    winid: data.winid,
                    checkval: true,
                    accepted: true,
                    promptvalue: responsePrompt
                }

                if (responsePrompt) {
                    webViewport.sendAsyncMessage("promptresponse", responseMessages[appWindow.testCaseNum])
                }
                appWindow.promptReceived = true
            } else if (message == "testembed:elementinnervalue") {
                appWindow.testResult = data.value
            }
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_prompt"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_prompt cleanupTestCase")

            // Clear no response case
            webViewport.sendAsyncMessage("promptresponse", responseMessages[2])
            wait(1000)
        }

        function test_1TestPromptPage() {
            MyScript.dumpTs("test_1TestPromptPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.testCaseNum = 0
            appWindow.promptReceived = false
            appWindow.testResult = null
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                                name: "result" })
            verify(MyScript.wrtWait(function() { return (!appWindow.testResult); }))
            compare(appWindow.testResult, "ok");
            MyScript.dumpTs("test_1TestPromptPage end");
        }

        function test_2TestPromptWithBadResponse() {
            MyScript.dumpTs("test_2TestPromptWithBadResponse start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.testCaseNum = 1
            appWindow.promptReceived = false
            appWindow.testResult = null
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                             name: "result" })
            verify(MyScript.wrtWait(function() { return (!appWindow.testResult); }))
            compare(appWindow.testResult, "failed");
            MyScript.dumpTs("test_2TestPromptWithBadResponse end");
        }

        function test_3TestPromptWithoutResponse() {
            MyScript.dumpTs("test_3TestPromptWithoutResponse start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.testCaseNum = 2
            appWindow.promptReceived = false
            appWindow.testResult = null
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                                name: "result" })
            verify(MyScript.wrtWait(function() { return (!appWindow.testResult); }))
            compare(appWindow.testResult, "unknown");
            MyScript.dumpTs("test_3TestPromptWithoutResponse end");
        }
    }
}
