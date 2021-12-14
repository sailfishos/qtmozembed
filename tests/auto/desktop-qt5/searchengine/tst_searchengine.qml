import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property var testResult
    property var testEngines
    property var testDefault

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QMozEngineSettings.setPreference("browser.search.defaultenginename", "QMOZTest")
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
            QMozEngineSettings.setPreference("browser.search.log", true)
            QmlMozContext.addObserver("browser-search-engine-modified")
            QmlMozContext.addObserver("embed:search")
            QMozEngineSettings.setPreference("keyword.enabled", true)
        }
        onRecvObserve: {
            if (message == "embed:search") {
                switch (data.msg) {
                    case "init": {
                        print("Received: search:" + message, ", msg: ", data.msg, data.defaultEngine)
                        appWindow.testEngines = data.engines
                        appWindow.testDefault = data.defaultEngine
                        break
                    }
                    case "search-engine-added": {
                        appWindow.testEngines.push(data.engine)
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
            wait(1000)
        }

        function test_TestCheckDefaultSearch() {
            var engineExistsPredicate = function() {
                var found = false;

                appWindow.testEngines.forEach(function(e) {
                    if (e === "QMOZTest") {
                        found = true;
                    }
                });

                return !found;
            };
            MyScript.dumpTs("TestCheckDefaultSearch start")
            verify(MyScript.waitMozContext())
            QMozEngineSettings.setPreference("browser.search.log", true);
            QmlMozContext.addObserver("browser-search-engine-modified");
            QmlMozContext.addObserver("embed:search");
            QMozEngineSettings.setPreference("keyword.enabled", true);
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
            verify(webViewport.url.toString().indexOf(appWindow.testDefault.toLowerCase()) !== -1)
            MyScript.dumpTs("TestCheckDefaultSearch end");
        }
    }
}
