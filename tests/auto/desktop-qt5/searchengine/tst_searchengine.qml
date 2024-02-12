import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import Nemo.Connectivity 1.0

import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property string testResult
    property var testEngines
    property string testDefault

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
            QMozEngineSettings.setPreference("browser.search.defaultenginename", "QMOZTest")
            QMozEngineSettings.setPreference("browser.search.log", true)
            QMozEngineSettings.setPreference("keyword.enabled", true)
            QmlMozContext.addObserver("browser-search-engine-modified")
            QmlMozContext.addObserver("embed:search")
        }
        onRecvObserve: {
            if (message == "embed:search") {
                switch (data.msg) {
                    case "init": {
                        print("Received: search:" + message, ", msg: ", data.msg, data.defaultEngine)
                        appWindow.testEngines = data.engines
                        appWindow.testDefault = data.defaultEngine || ""
                        break
                    }
                    case "search-engine-added": {
                        appWindow.testEngines.push(data.engine)

                        break
                    }
                    case "search-engine-default-changed": {
                        print("Default search engine changed:", data.msg, data.defaultEngine)
                        appWindow.testDefault = data.defaultEngine || ""
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

    ConnectionHelper {
        id: connectionHelper
        readonly property bool connected: status >= ConnectionHelper.Connected
    }

    SignalSpy {
        id: initializedSpy
        target: QmlMozContext
        signalName: "initialized"
    }

    TestCase {
        id: testcaseid
        name: "tst_searchengine"
        when: windowShown

        function initTestCase() {
            QmlMozContext.addObserver("embed:search");
            QMozEngineSettings.setPreference("keyword.enabled", true);

            if (!QmlMozContext.isInitialized()) {
                initializedSpy.wait()
            }
        }

        function cleanupTestCase() {
            MyScript.dumpTs("tst_searchengine cleanupTestCase")
            wait(1000)
        }

        function test_001_addSearchEngine() {
            var engineExistsPredicate = function() {
                var found = false;

                appWindow.testEngines.forEach(function(e) {
                    MyScript.dumpTs("Engine:", e)
                    if (e === "QMOZTest") {
                        found = true;
                    }
                });

                return !found;
            };
            MyScript.dumpTs("AddSearchEngine start")
            verify(MyScript.waitMozContext())

            verify(MyScript.waitMozView())
            QmlMozContext.notifyObservers("embedui:search", {msg:"remove", name: "QMOZTest"})
            verify(MyScript.wrtWait(function() { return (!engineExistsPredicate()); }))
            QmlMozContext.notifyObservers("embedui:search", {msg:"loadxml", uri: "file://" + TestHelper.getenv("QTTESTSROOT") + "/auto/shared/searchengine/test.xml", confirm: false})
            verify(MyScript.wrtWait(function() { return (appWindow.testResult !== "loaded"); }))
            verify(MyScript.wrtWait(engineExistsPredicate));
            MyScript.dumpTs("AddSearchEngine end");
        }

        function test_002_setDefaultSearchEngine() {
            MyScript.dumpTs("setDefaultSearchEngine start")
            QmlMozContext.notifyObservers("embedui:search", { msg: "setdefault", name: "QMOZTest"})
            verify(MyScript.wrtWait(function() { return (appWindow.testDefault !== "QMOZTest"); }))
            MyScript.dumpTs("setDefaultSearchEngine end")
        }

        function test_003_searchFromWeb() {
            MyScript.dumpTs("searchFromWeb start")

            if (!connectionHelper.connected) {
                skip("searching with a search engine requires network access.")
            }

            webViewport.load("QMOZTest", false);
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))

            compare(webViewport.url.toString(), "https://qmoztest/?search=qmoztest")

            MyScript.dumpTs("searchFromWeb end")
        }
    }
}
