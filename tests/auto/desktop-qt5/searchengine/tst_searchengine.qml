import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var testResult

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onOnInitialized: {
            mozContext.instance.setPref("browser.search.defaultenginename", "QMOZTest")
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
            mozContext.instance.setPref("browser.search.log", true)
            mozContext.instance.addObserver("browser-search-engine-modified")
            mozContext.instance.addObserver("embed:search")
            mozContext.instance.setPref("keyword.enabled", true)
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

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_searchengine cleanup")
        }

        function test_TestCheckDefaultSearch()
        {
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
            mozContext.dumpTS("TestCheckDefaultSearch start")
            verify(MyScript.waitMozContext())
            mozContext.instance.setPref("browser.search.log", true);
            mozContext.instance.addObserver("browser-search-engine-modified");
            mozContext.instance.addObserver("embed:search");
            mozContext.instance.setPref("keyword.enabled", true);
            verify(MyScript.waitMozView())
            mozContext.instance.notifyObservers("embedui:search", {msg:"remove", name: "QMOZTest"})
            verify(MyScript.wrtWait(function() { return (!engineExistsPredicate()); }))
            mozContext.instance.notifyObservers("embedui:search", {msg:"loadxml", uri: "file://" + mozContext.getenv("QTTESTSROOT") + "/auto/shared/searchengine/test.xml", confirm: false})
            verify(MyScript.wrtWait(function() { return (appWindow.testResult !== "loaded"); }))
            mozContext.instance.notifyObservers("embedui:search", {msg:"getlist"})
            verify(MyScript.wrtWait(engineExistsPredicate));
            webViewport.load("linux home");
            verify(MyScript.waitLoadFinished(webViewport))
            compare(webViewport.loadProgress, 100);
            verify(MyScript.wrtWait(function() { return (!webViewport.painted); }))
            compare(webViewport.url.toString().substr(0, 34), "https://webhook/?search=linux+home")
            mozContext.dumpTS("TestCheckDefaultSearch end");
        }
    }
}
