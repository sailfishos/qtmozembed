import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var testResult

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.setPref("browser.search.defaultenginename", "QMOZTest")
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
            QmlMozContext.setPref("browser.search.log", true)
            QmlMozContext.addObserver("browser-search-engine-modified")
            QmlMozContext.addObserver("embed:search")
            QmlMozContext.setPref("keyword.enabled", true)
        }
        onRecvObserve: {
            if (message == "embed:search") {
                switch (data.msg) {
                    case "init": {
                        print("Received: search:" + message, ", msg: ", data.msg, data.defaultEngine)
                        break
                    }
                    case "pluginslist": {
                        for (var i = 0; i < data.list.length; ++i) {
                            print("Received: search:" + message, ", msg: ", data.msg, data.list[i].name, data.list[i].isDefault, data.list[i].isCurrent)
                        }
                        appWindow.testResult = data.list
                        break
                    }
                }
            } else if (message == "browser-search-engine-modified") {
                if (data == "engine-loaded") {
                    appWindow.testResult = "loaded"
                }
                print("Received: search mod:", data)
            }
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        onViewInitialized: {
            appWindow.mozViewInitialized = true
        }
        onRecvAsyncMessage: {
            print("onRecvAsyncMessage:" + message + ", data:" + data)
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_searchengine"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_searchengine cleanupTestCase")
        }

        function test_TestCheckDefaultSearch() {
            var engineExistsPredicate = function() {
                var found = false;

                if (!Array.isArray(appWindow.testResult)) {
                    return true;
                }

                appWindow.testResult.forEach(function(e) {
                    if (e.name === "QMOZTest") {
                        found = true;
                    }
                });

                return !found;
            };
            MyScript.dumpTs("TestCheckDefaultSearch start")
            verify(MyScript.waitMozContext())
            QmlMozContext.setPref("browser.search.log", true);
            QmlMozContext.addObserver("browser-search-engine-modified");
            QmlMozContext.addObserver("embed:search");
            QmlMozContext.setPref("keyword.enabled", true);
            verify(MyScript.waitMozView())
            QmlMozContext.notifyObservers("embedui:search", {msg:"remove", name: "QMOZTest"})
            verify(MyScript.wrtWait(function() { return (!engineExistsPredicate()); }))
            QmlMozContext.notifyObservers("embedui:search", {msg:"loadxml", uri: "file://" + TestHelper.getenv("QTTESTSROOT") + "/auto/shared/searchengine/test.xml", confirm: false})
            verify(MyScript.wrtWait(function() { return (appWindow.testResult !== "loaded"); }))
            QmlMozContext.notifyObservers("embedui:search", {msg:"getlist"})
            verify(MyScript.wrtWait(engineExistsPredicate));
            webViewport.load("linux home");
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            compare(webViewport.url.toString().substr(0, 34), "https://webhook/?search=linux+home")
            MyScript.dumpTs("TestCheckDefaultSearch end");
        }
    }
}
