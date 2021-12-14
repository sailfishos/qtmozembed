import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onRecvObserve: console.log("onRecvObserve: msg:", message, ", name:", data.name, "value:", data.value)
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            appWindow.mozViewInitialized = true
            MyScript.dumpTs("tst_activatelinks onViewInitialized")
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_activatelinks"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_activatelinks cleanup")
        }

        function test_ActiveHyperLink() {
            MyScript.dumpTs("test_ActiveHyperLink start")
            verify(MyScript.waitMozContext())
            QmlMozContext.notifyObservers("embedui:setprefs", { prefs :
            [
                { name: "embedlite.azpc.handle.singletap", value: false},
                { name: "embedlite.azpc.json.singletap", value: true},
                { name: "embedlite.azpc.handle.longtap", value: false},
                { name: "embedlite.azpc.json.longtap", value: true},
                { name: "embedlite.azpc.json.viewport", value: true},
                { name: "browser.ui.touch.left", value: 32},
                { name: "browser.ui.touch.right", value: 32},
                { name: "browser.ui.touch.top", value: 48},
                { name: "browser.ui.touch.bottom", value: 16},
                { name: "browser.ui.touch.weight.visited", value: 120}
            ]});
            verify(MyScript.waitMozView())
            webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body><a href=about:blank>ActiveLink</a>";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            mouseClick(webViewport, 10, 20)
            verify(MyScript.wrtWait(function() { return webViewport.url != "about:blank"; }))
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            QmlMozContext.notifyObservers("embedui:setprefs", { prefs :
            [
                { name: "embedlite.azpc.handle.singletap", value: true},
                { name: "embedlite.azpc.json.singletap", value: false},
                { name: "embedlite.azpc.handle.longtap", value: true},
                { name: "embedlite.azpc.json.longtap", value: false},
                { name: "embedlite.azpc.json.viewport", value: false},
                { name: "browser.ui.touch.left", value: 32},
                { name: "browser.ui.touch.right", value: 32},
                { name: "browser.ui.touch.top", value: 48},
                { name: "browser.ui.touch.bottom", value: 16},
                { name: "browser.ui.touch.weight.visited", value: 120}
            ]});
            MyScript.dumpTs("test_ActiveHyperLink end")
        }
    }
}
