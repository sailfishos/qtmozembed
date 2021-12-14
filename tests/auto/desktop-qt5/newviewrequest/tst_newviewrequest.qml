import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    property var oldMozView

    name: testcaseid.name

    WebViewCreator {
        parentItem: appWindow
        webViewComponent: Qt.createComponent(TestHelper.getenv("QTTESTSROOT") + "/auto/shared/ViewComponent.qml")
        onAboutToCreateNewView: {
              appWindow.oldMozView = appWindow.mozView
              appWindow.mozView = null
        }
        onNewViewCreated: {
            testcaseid.verify(view)

            print("New Window created: url: ", view.url, ", parentID:", view.parentId)
            appWindow.createParentID = view.parentId
            appWindow.mozView = view
            testcaseid.verify(mozView.uniqueId > 0)
            view.viewInitialized.connect(function() {
                mozViewInitialized = true
            })
        }
    }

    TestCase {
        id: testcaseid
        name: "tst_newviewrequest"
        when: windowShown

        function cleanupTestCase() {
            MyScript.dumpTs("tst_newviewrequest cleanupTestCase")
            wait(1000)
        }

        function test_1newcontextPrepareViewContext() {
            MyScript.dumpTs("test_1newcontextPrepareViewContext start")
            verify(MyScript.wrtWait(function() { return (QmlMozContext.isInitialized() === false); }, 100, 500))
            verify(QmlMozContext.isInitialized())
            MyScript.dumpTs("test_1newcontextPrepareViewContext end")
        }

        function test_2newviewInit() {
            MyScript.dumpTs("test_2newviewInit start")
            verify(MyScript.wrtWait(function() { return (QmlMozContext.isInitialized() === false); }, 100, 500))
            verify(QmlMozContext.isInitialized())
            MyScript.createSpriteObjects();
            verify(MyScript.wrtWait(function() { return (mozView === null); }, 10, 500))
            verify(MyScript.wrtWait(function() { return (mozViewInitialized !== true); }, 10, 500))
            verify(mozView !== undefined)
            MyScript.dumpTs("test_2newviewInit end")
        }

        function test_3viewTestNewWindowAPI() {
            MyScript.dumpTs("test_viewTestNewWindowAPI start")
            verify(MyScript.wrtWait(function() { return (mozView === undefined); }, 100, 500))
            verify(mozView !== undefined)
            mozView.url = TestHelper.getenv("QTTESTSROOT") + "/auto/shared/newviewrequest/newwin.html";
            verify(MyScript.waitLoadFinished(mozView))
            compare(mozView.title, "NewWinExample")
            verify(MyScript.wrtWait(function() { return (!mozView.painted); }))
            mozViewInitialized = false
            mouseClick(mozView, 10, 10)
            verify(MyScript.wrtWait(function() { return (mozViewInitialized !== true); }))
            verify(mozView !== undefined)
            waitForRendering(mozView, 1000)
            verify(MyScript.waitLoadFinished(mozView))
            compare(mozView.title, "Created window")
            MyScript.dumpTs("test_viewTestNewWindowAPI end")
            wait(1000)
        }
    }

    Component.onCompleted: {
        QMozEngineSettings.setPreference("embedlite.azpc.handle.singletap", false);
        QMozEngineSettings.setPreference("embedlite.azpc.json.singletap", true);
    }
}
