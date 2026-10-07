/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 * SPDX-License-Identifier: MPL-2.0
 */

import QtQuick 2.6
import QtQuick.Window 2.2
import QtTest 1.0
import Qt5Mozilla 1.0

Item {
    id: root

    width: 540
    height: 960
    property bool windowReady: Window.window !== null
    property int destructionCount
    property bool testReady

    Timer {
        interval: 0
        running: root.windowReady
        onTriggered: root.testReady = true
    }

    Component {
        id: viewComponent

        QmlMozView {
            property int initializationCount

            onViewInitialized: ++initializationCount
            Component.onDestruction: ++root.destructionCount
        }
    }

    TestCase {
        name: "tst_viewlifecycle"
        when: root.testReady

        function test_attachAfterCompletion() {
            for (var attempt = 0; attempt < 100 && !QmlMozContext.isInitialized(); ++attempt) {
                wait(100)
            }
            verify(QmlMozContext.isInitialized())

            // Destruction must cancel a queued creation attempt.
            var discarded = viewComponent.createObject(null)
            verify(discarded !== null)
            discarded.destroy()
            wait(50)

            // Complete the component without a size or a containing window.
            var view = viewComponent.createObject(root, {"parent": null})
            verify(view !== null)
            wait(50)
            compare(view.initializationCount, 0)
            compare(view.width, 0)
            compare(view.height, 0)

            // Window attachment must retry using the window's size.
            view.parent = root
            tryCompare(view, "initializationCount", 1, 10000)

            // Further geometry notifications must not create another view.
            view.width = root.width
            view.height = root.height
            view.url = "data:text/html,<title>Deferred view</title>Ready"
            tryCompare(view, "loadProgress", 100, 10000)
            tryCompare(view, "loading", false, 10000)
            compare(view.initializationCount, 1)
            // Keep the last view alive until the test window is torn down.
            // Its destruction then stops embedding after quick_test_main finishes.
        }
        function test_repeatedNavigationAndDestruction() {
            for (var i = 0; i < 10; ++i) {
                var before = root.destructionCount
                var pending = viewComponent.createObject(null)
                pending.destroy()
                tryCompare(root, "destructionCount", before + 1)

                var view = viewComponent.createObject(root, {"width": 300, "height": 300, "active": true})
                tryCompare(view, "initializationCount", 1, 10000)
                var title = "Lifecycle " + i
                view.url = "data:text/html,<title>" + title + "</title>Ready"
                tryCompare(view, "title", title, 10000)
                tryCompare(view, "loading", false, 10000)
                var callbacks = 0
                var result
                view.runJavaScript("return document.title", function(value) {
                    ++callbacks
                    result = value
                })
                for (var tick = 0; tick < 100 && callbacks === 0; ++tick)
                    wait(50)
                compare(result, title)
                compare(callbacks, 1)
                view.url = "data:text/html,<title>Replacement</title>New document"
                view.destroy()
                tryCompare(root, "destructionCount", before + 2)
                wait(50)
                compare(callbacks, 1)
            }
        }
    }
}
