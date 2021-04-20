import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

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
            mozContext.dumpTS("test_TestPromptPage start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.testCaseNum = 0
            appWindow.promptReceived = false
            appWindow.testResult = null
            webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                                name: "result" })
            verify(MyScript.wrtWait(function() { return (!appWindow.testResult); }))
            compare(appWindow.testResult, "ok");
            mozContext.dumpTS("test_TestPromptPage end");
        }

        function test_TestPromptWithBadResponse()
        {
            mozContext.dumpTS("test_TestPromptWithBadResponse start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.testCaseNum = 1
            appWindow.promptReceived = false
            appWindow.testResult = null
            webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                             name: "result" })
            verify(MyScript.wrtWait(function() { return (!appWindow.testResult); }))
            compare(appWindow.testResult, "failed");
            mozContext.dumpTS("test_TestPromptWithBadResponse end");
        }

        function test_TestPromptWithoutResponse()
        {
            mozContext.dumpTS("test_TestPromptWithoutResponse start")
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.testCaseNum = 2
            appWindow.promptReceived = false
            appWindow.testResult = null
            webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            verify(MyScript.wrtWait(function() { return (!appWindow.promptReceived); }))
            webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                                name: "result" })
            verify(MyScript.wrtWait(function() { return (!appWindow.testResult); }))
            compare(appWindow.testResult, "unknown");
            mozContext.dumpTS("test_TestPromptWithoutResponse end");
        }
    }
}
