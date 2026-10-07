import QtTest 1.0
import QtQuick 2.6
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property var loginPrompt
    property var fieldState
    property var savedLogins

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onRecvObserve: {
            if (message === "embed:all-logins")
                appWindow.savedLogins = data
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
            webViewport.addMessageListener("embed:login")
        }
        onRecvAsyncMessage: {
            print("onRecvAsyncMessage:" + message + ", data:" + data)
            if (message == "embed:login") {
                appWindow.loginPrompt = data
            }
        }
    }

    TestCase {
        id: testcaseid

        name: "tst_passwordmgr"
        when: windowShown

        function initTestCase() {
            verify(MyScript.waitMozContext())
            QmlMozContext.addObserver("embed:all-logins")
        }

        function getLogins() {
            appWindow.savedLogins = null
            QmlMozContext.notifyObservers("embedui:logins", {action: "getall"})
            verify(MyScript.wrtWait(function() { return appWindow.savedLogins === null }, 100, 50))
            return appWindow.savedLogins
        }

        function test_TestLoginMgrPage_data() {
            return [{tag: "cancel", button: 2, saved: false},
                    {tag: "save", button: 0, saved: true}]
        }

        function cleanupTestCase() {
            MyScript.dumpTs("tst_passwordmgr cleanupTestCase")
        }

        function enterText(id, x, y, keys, value) {
            appWindow.fieldState = null
            for (var attempt = 0; attempt < 20; ++attempt) {
                mouseClick(webViewport, x, y)
                webViewport.runJavaScript("return document.activeElement.id", function(result) {
                    appWindow.fieldState = result
                })
                wait(50)
                if (appWindow.fieldState === id)
                    break
            }
            compare(appWindow.fieldState, id)
            for (var i = 0; i < keys.length; ++i)
                keyClick(keys[i])
            appWindow.fieldState = null
            for (var retry = 0; retry < 100 && appWindow.fieldState !== value; ++retry) {
                webViewport.runJavaScript("return document.getElementById('" + id + "').value", function(result) {
                    appWindow.fieldState = result
                })
                wait(50)
            }
            compare(appWindow.fieldState, value)
        }

        function test_TestLoginMgrPage(data) {
            verify(MyScript.waitMozContext())
            verify(MyScript.waitMozView())
            appWindow.loginPrompt = null
            webViewport.url = TestHelper.serveFile(TestHelper.getenv("QTTESTSROOT")
                    + "/auto/shared/passwordmgr/subtst_notifications_1.html")
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100)
            verify(MyScript.wrtWait(function() { return !webViewport.painted }))
            // Login capture requires real user interaction with the form.
            enterText("user", 30, 25, [Qt.Key_K, Qt.Key_O, Qt.Key_R, Qt.Key_P], "korp")
            enterText("pass", 30, 75, [Qt.Key_P, Qt.Key_A, Qt.Key_S, Qt.Key_S], "pass")
            mouseClick(webViewport, 30, 125)
            verify(MyScript.wrtWait(function() { return !appWindow.loginPrompt }, 100, 50))
            compare(appWindow.loginPrompt.name, "password-save")
            verify(appWindow.loginPrompt.id.length > 0)
            webViewport.sendAsyncMessage("embedui:login", {
                buttonidx: data.button,
                id: appWindow.loginPrompt.id
            })
            var logins = getLogins()
            if (data.saved) {
                for (var retry = 0; retry < 100 && logins.length === 0; ++retry) {
                    wait(50)
                    logins = getLogins()
                }
                compare(logins.length, 1)
                compare(logins[0].username, "korp")
                compare(logins[0].password, "pass")
                QmlMozContext.notifyObservers("embedui:logins", {action: "remove", login: logins[0]})
                for (var removal = 0; removal < 100 && getLogins().length !== 0; ++removal)
                    wait(50)
                compare(getLogins().length, 0)
            } else {
                compare(logins.length, 0)
                // A later observer request must still see no persisted login.
                wait(100)
                compare(getLogins().length, 0)
            }
        }
    }
}
