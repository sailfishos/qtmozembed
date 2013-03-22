import QtQuickTest 1.0
import QtQuick 1.0
import Sailfish.Silica 1.0
import QtMozilla 1.0
import "scripts/Util.js" as Util

ApplicationWindow {
    id: window
    property string currentPageName: pageStack.currentPage != null
            ? pageStack.currentPage.objectName
            : ""

    QmlMozContext { id: mozContext }

    initialPage: Page {
        id: startPage
        width: 480
        height: 854
        objectName: "startPage"
        allowedOrientations: Orientation.Portrait
        orientation: Orientation.Portrait
    }

/*
    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        anchors.fill: parent
        Connections {
            target: webViewport.child
            onViewInitialized: {
                print("QML View Initialized")
                 webViewport.child.url = "about:mozilla"
            }
        }
    }
*/

    TestCase {
        name: "StartPage"
        when: windowShown


        function cleanup() {
            // Clear all items.
            // Return to the start page
            window.pageStack.pop(null, PageStackAction.Immediate)
        }

        function test_initialize() {
            var testval = 1;
            verify(testval !== undefined)
            mozContext.init();
            verify(mozContext.init.child == undefined)
//            wait(10);
        }
    }
}
