import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import qtmozembed.tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared/sharedTests.js" as SharedTests

Item {
    id: appWindow
    width: 480
    height: 800

    property bool mozViewInitialized
    property var mozView
    property var oldMozView
    property int createParentID

    QmlMozContext {
        id: mozContext
    }

    WebViewCreator {
        onNewWindowRequested: {
            print("New Window Requested: url: ", url, ", parentID:", parentId);
            appWindow.oldMozView = appWindow.mozView;
            appWindow.mozView = null;
            appWindow.createParentID = parentId;
            MyScript.createSpriteObjects();
            while (appWindow.mozView === null) {
                testcaseid.wait()
            }
            testcaseid.verify(mozView.uniqueID() > 0)
        }
    }

    Connections {
        target: mozContext.instance
        onOnInitialized: {
            // Gecko does not switch to SW mode if gl context failed to init
            // and qmlmoztestrunner does not build in GL mode
            // Let's put it here for now in SW mode always
            mozContext.instance.setIsAccelerated(true);
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown
        parent: appWindow

        function cleanup() {
            mozContext.dumpTS("tst_newviewrequest cleanup")
        }

        function test_1newcontextPrepareViewContext()
        {
            SharedTests.shared_1newcontextPrepareViewContext()
        }
        function test_2newviewInit()
        {
            SharedTests.shared_2newviewInit()
        }
        function test_viewTestNewWindowAPI()
        {
            SharedTests.shared_viewTestNewWindowAPI()
        }
    }
}
