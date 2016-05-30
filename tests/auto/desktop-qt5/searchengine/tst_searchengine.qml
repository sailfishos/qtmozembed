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
    property var testResult

    QmlMozContext {
        id: mozContext
    }
    Connections {
        target: mozContext.instance
        onOnInitialized: {
            // Gecko does not switch to SW mode if gl context failed to init
            // and qmlmoztestrunner does not build in GL mode
            // Let's put it here for now in SW mode always
            mozContext.instance.setIsAccelerated(true);
            mozContext.instance.setPref("browser.search.defaultenginename", "QMOZTest");
            mozContext.instance.addComponentManifest(mozContext.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest");
            mozContext.instance.setPref("browser.search.log", true);
            mozContext.instance.addObserver("browser-search-engine-modified");
            mozContext.instance.addObserver("embed:search");
            mozContext.instance.setPref("keyword.enabled", true);
        }
        onRecvObserve: {
            if (message == "embed:search") {
                switch (data.msg) {
                    case "init": {
                        print("Received: search:" + message, ", msg: ", data.msg, data.defaultEngine);
                        break;
                    }
                    case "pluginslist": {
                        for (var i = 0; i < data.list.length; ++i) {
                            print("Received: search:" + message, ", msg: ", data.msg, data.list[i].name, data.list[i].isDefault, data.list[i].isCurrent);
                        }
                        appWindow.testResult = data.list;
                        break;
                    }
                }
            } else if (message == "browser-search-engine-modified") {
                if (data == "engine-loaded") {
                    appWindow.testResult = "loaded";
                }
                print("Received: search mod:", data);
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
            SharedTests.shared_TestCheckDefaultSearch()
        }
    }
}
