import QtQuickTest 1.0
import QtQuick 1.0
import Sailfish.Silica 1.0
import QtMozilla 1.0
import "componentCreation.js" as MyScript

ApplicationWindow {
    id: appWindow

    property string currentPageName: pageStack.currentPage != null
            ? pageStack.currentPage.objectName
            : ""

    property bool mozViewInitialized : false
    property variant mozView : null

    QmlMozContext {
        id: mozContext
        autoinit: false
    }
    Connections {
        target: mozContext.child
        onOnInitialized: {
            // Gecko does not switch to SW mode if gl context failed to init
            // and qmlmoztestrunner does not build in GL mode
            // Let's put it here for now in SW mode always
            mozContext.child.setIsAccelerated(false);
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown

        function cleanup() {
            // Return to the start page
            appWindow.pageStack.pop(null, PageStackAction.Immediate)
        }

        function test_1contextPrepareViewContext()
        {
            verify(mozContext.child !== undefined)
            while (mozContext.child.initialized() === false) {
                wait(500)
            }
            verify(mozContext.child.initialized())
        }
        function test_2viewInit()
        {
            verify(mozContext.child.initialized())
            MyScript.createSpriteObjects();
            while (mozView === null) {
                wait(500)
            }
            while (mozViewInitialized !== true) {
                wait(500)
            }
            verify(mozView.child !== undefined)
        }
        function test_3viewLoadURL()
        {
            verify(mozView.child !== undefined)
            mozView.child.url = "about:mozilla";
            verify(MyScript.waitLoadStarted(mozView))
            verify(MyScript.waitLoadFinished(mozView))
            compare(mozView.child.url, "about:mozilla")
        }
    }
}
