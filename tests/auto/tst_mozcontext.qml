import QtQuickTest 1.0
import QtQuick 1.0
import Sailfish.Silica 1.0
import QtMozilla 1.0

ApplicationWindow {
    id: appWindow

    QmlMozContext {
        id: mozContext
        autoinit: false
    }
    Connections {
        target: mozContext.child
        onOnInitialized: {
            testcaseid.verify(mozContext.child.initialized() == true)
            mozContext.child.setIsAccelerated(false);
            testcaseid.verify(mozContext.child.isAccelerated() == false)
            mozContext.child.setIsAccelerated(true);
            testcaseid.verify(mozContext.child.isAccelerated() == true)
            mozContext.child.stopEmbedding();
        }
    }

    resources: TestCase {
        id: testcaseid
        name: "mozContextPage"
        when: windowShown

        function test_contextInit()
        {
            verify(mozContext.child !== undefined)
            // No way out of here until mozContext.child.stopEmbedding called
            mozContext.child.runEmbedding();
        }
    }
}
