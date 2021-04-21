.import QtQml 2.2 as Qml
.import QtMozEmbed.Tests 1.0 as QtMozEmbed
.import Qt5Mozilla 1.0 as Qt5Mozilla

var component;
var sprite;
var context_view_wait_timeout = 1000;
var load_finish_wait_timeout = 10000;

function createSpriteObjects() {
    component = Qt.createComponent(QtMozEmbed.TestHelper.getenv("QTTESTSROOT") + "/auto/shared/ViewComponent.qml");
    if (component.status === Qml.Component.Ready) {
        finishCreation();
    }
    else {
        component.statusChanged.connect(finishCreation);
    }
}

function finishCreation() {
    if (component.status === Qml.Component.Ready) {
        appWindow.mozView = component.createObject(appWindow, {"x": 0, "y": 0, "active": true});
        if (!appWindow.mozView) {
            // Error Handling
            console.log("Error creating object");
        }
    } else if (component.status === Qml.Component.Error) {
        // Error Handling
        console.log("Error loading component:", component.errorString());
    }
}

function waitMozContext() {
    if (Qt5Mozilla.QmlMozContext.isInitialized()) {
        return true;
    }
    var ticks = 0;
    while (!Qt5Mozilla.QmlMozContext.isInitialized() && ticks++ < context_view_wait_timeout) {
        testcaseid.wait();
    }
    return ticks < context_view_wait_timeout;
}

function waitMozView() {
    if (appWindow.mozViewInitialized) {
        return true;
    }
    var ticks = 0;
    while (!appWindow.mozViewInitialized && ticks++ < context_view_wait_timeout) {
        testcaseid.wait();
    }
    return ticks < context_view_wait_timeout;
}

function waitLoadFinished(view) {
    if (!view.loading && view.loadProgress !== 0) {
        return true;
    }
    var ticks = 0;
    while ((view.loading || view.loadProgress === 0) && ticks++ < load_finish_wait_timeout) {
        testcaseid.wait();
    }
    return ticks < load_finish_wait_timeout;
}

function wrtWait(conditionFunc, waitIter, timeout) {
    timeout = typeof timeout !== 'undefined' ? timeout : -1;
    waitIter = typeof waitIter !== 'undefined' ? waitIter : 5000;
    var tick = 0;
    while (conditionFunc() && tick++ < waitIter) {
        if (timeout === -1) {
            testcaseid.wait()
        }
        else {
            testcaseid.wait(timeout)
        }
    }
    return tick < waitIter;
}

function dumpTs(message) {
    print("TimeStamp:" + message + ", " + Qt.formatTime(new Date(), "hh:mm:ss::ms"));
}

function scrollBy(startX, startY, dx, dy, timeMs, isKinetic)
{
    var frameMs = 16;
    var timeMsStep = timeMs / frameMs;
    var stepRX = dx / timeMsStep;
    var stepRY = dy / timeMsStep;
    var curRX = startX;
    var curRY = startY;
    var endRX = curRX + dx;
    var endRY = curRY + dy;
    testcaseid.mousePress(webViewport, curRX, curRY, 1);
    while (curRX !== endRX || curRY !== endRY) {
        curRX = stepRX > 0 ? Math.min(curRX + stepRX, endRX) : Math.max(curRX + stepRX, endRX);
        curRY = stepRY > 0 ? Math.min(curRY + stepRY, endRY) : Math.max(curRY + stepRY, endRY);
        testcaseid.mouseMove(webViewport, curRX, curRY, -1, 1);
    }
    testcaseid.mouseRelease(webViewport, curRX, curRY, 1);
    testcaseid.mousePress(webViewport, curRX, curRY, 1);
    testcaseid.mouseRelease(webViewport, curRX, curRY, 1);
}
