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

    FileInfo {
        id: thumbnailHelper
    }

    initialPage: GalleryStartPage {
        id: startPage
        width: 480
        height: 854
        objectName: "startPage"
        allowedOrientations: Orientation.Portrait
        orientation: Orientation.Portrait
    }

    TestCase {
        name: "StartPage"
        when: windowShown


        function cleanup() {
            // Clear all items.
            testService.updateGraph("http://www.tracker-project.org/temp/nmm#ImageList")

            // Return to the start page
            window.pageStack.pop(null, PageStackAction.Immediate)
        }

        function test_openAlbum() {
            var delegate = Util.findItem(startPage, function(item) {
                return item.objectName == "titleLabel" && item.text == qsTrId("gallery-bt-photos")
            }).parent
            verify(delegate !== undefined)

            mouseClick(delegate, delegate.width / 2, delegate.height / 2)
            tryCompare(window.pageStack, "busy", false)

            tryCompare(window, "currentPageName", "gridPage")

            // Use the view count to verify the correct page has been loaded.
            var gridView = Util.findItemByName(window.pageStack.currentPage, "gridView")
            verify(gridView !== undefined)
            tryCompare(gridView, "count", 2)
        }
    }
}
