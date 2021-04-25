import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onRecvObserve: {
            print("onRecvObserve: msg:", message, ", data:", data.data)
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            webViewport.useQmlMouse = true
            appWindow.mozViewInitialized = true
            mozContext.dumpTS("tst_activatelinks onViewInitialized")
        }
    }

    MouseArea {
        id: viewportMouse
        anchors.fill: parent
        onPressed: webViewport.recvMousePress(mouseX, mouseY)
        onReleased: webViewport.recvMouseRelease(mouseX, mouseY)
        onPositionChanged: webViewport.recvMouseMove(mouseX, mouseY)
    }

    TestCase {
        id: testcaseid
        name: "tst_activatelinks"
        when: windowShown

        function cleanupTestCase() {
            mozContext.dumpTS("tst_activatelinks cleanup")
        }

        function test_ActiveHyperLink() {
            mozContext.dumpTS("test_ActiveHyperLink start")
            verify(MyScript.waitMozContext())
            mozContext.instance.notifyObservers("embedui:setprefs", { prefs :
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
            webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body><a href=about:license>ActiveLink</a>";
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            mouseClick(webViewport, 10, 20)
            verify(MyScript.wrtWait(function() { return webViewport.url != "about:license"; }))
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            mozContext.instance.notifyObservers("embedui:setprefs", { prefs :
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
            mozContext.dumpTS("test_ActiveHyperLink end")
            webViewport.useQmlMouse = false
        }
    }
}
