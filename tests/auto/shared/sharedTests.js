var component;

// Helper functions
function wrtWait(conditionFunc, waitIter, timeout)
{
    timeout = typeof timeout !== 'undefined' ? timeout : -1;
    waitIter = typeof waitIter !== 'undefined' ? waitIter : 5000;
    var tick = 0;
    while (conditionFunc() && tick++ < waitIter) {
        if (timeout == -1) {
            testcaseid.wait()
        }
        else {
            testcaseid.wait(timeout)
        }
    }
    return tick < waitIter;
}

// Shared Tests
function shared_context1Init()
{
    mozContext.dumpTS("test_context1Init start")
    testcaseid.verify(mozContext.instance !== undefined)
    testcaseid.verify(wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
    testcaseid.verify(mozContext.instance.isInitialized())
    mozContext.dumpTS("test_context1Init end")
}
function shared_context3PrefAPI()
{
    mozContext.dumpTS("test_context3PrefAPI start")
    mozContext.instance.setPref("test.embedlite.pref", "result");
    mozContext.dumpTS("test_context3PrefAPI end")
}
function shared_context4ObserveAPI()
{
    mozContext.dumpTS("test_context4ObserveAPI start")
    mozContext.instance.sendObserve("memory-pressure", null);
    mozContext.instance.addObserver("test-observe-message");
    mozContext.instance.sendObserve("test-observe-message", {msg: "testMessage", val: 1});
    testcaseid.verify(wrtWait(function() { return (lastObserveMessage === undefined); }, 10, 500))
    testcaseid.compare(lastObserveMessage.msg, "test-observe-message");
    testcaseid.compare(lastObserveMessage.data.val, 1);
    testcaseid.compare(lastObserveMessage.data.msg, "testMessage");
    mozContext.dumpTS("test_context4ObserveAPI end")
}
function shared_Test1LoadInputPage()
{
    mozContext.dumpTS("test_Test1LoadInputPage start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body><input id=myelem value=''>";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.mouseClick(webViewport, 10, 10)
    testcaseid.verify(wrtWait(function() { return (!appWindow.isState(1, 0, 3)); }))
    appWindow.inputState = false;
    testcaseid.keyClick(Qt.Key_K);
    testcaseid.keyClick(Qt.Key_O);
    testcaseid.keyClick(Qt.Key_R);
    testcaseid.keyClick(Qt.Key_P);
    webViewport.sendAsyncMessage("embedtest:getelementprop", {
                                        name: "myelem",
                                        property: "value"
                                       })
    testcaseid.verify(wrtWait(function() { return (appWindow.inputContent == ""); }))
    testcaseid.compare(appWindow.inputContent, "korp");
    mozContext.dumpTS("test_Test1LoadInputPage end");
}

function shared_Test1LoadInputURLPage()
{
    mozContext.dumpTS("test_Test1LoadInputPage start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    appWindow.inputContent = ""
    appWindow.inputType = ""
    webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body><input type=number id=myelem value=''>";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.mouseClick(webViewport, 10, 10)
    testcaseid.verify(wrtWait(function() { return (!appWindow.isState(1, 0, 3)); }))
    appWindow.inputState = false;
    testcaseid.keyClick(Qt.Key_1);
    testcaseid.keyClick(Qt.Key_2);
    testcaseid.keyClick(Qt.Key_3);
    testcaseid.keyClick(Qt.Key_4);
    webViewport.sendAsyncMessage("embedtest:getelementprop", {
                                        name: "myelem",
                                        property: "value"
                                       })
    testcaseid.verify(wrtWait(function() { return (appWindow.inputContent == ""); }))
    testcaseid.verify(wrtWait(function() { return (appWindow.inputType == ""); }))
    testcaseid.compare(appWindow.inputContent, "1234");
    mozContext.dumpTS("test_Test1LoadInputPage end");
}

function shared_TestScrollPaintOperations()
{
    mozContext.dumpTS("test_TestScrollPaintOperations start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body bgcolor=red leftmargin=0 topmargin=0 marginwidth=0 marginheight=0><input style='position:absolute; left:0px; top:1200px;'>";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    while (appWindow.scrollY === 0) {
        MyScript.scrollBy(100, 301, 0, -200, 100, false);
        testcaseid.wait(100);
    }
    testcaseid.verify(appWindow.scrollX === 0)
    mozContext.dumpTS("test_TestScrollPaintOperations end");
}

function shared_1contextPrepareViewContext()
{
    mozContext.dumpTS("test_1contextPrepareViewContext start")
    testcaseid.verify(mozContext.instance !== undefined)
    testcaseid.verify(wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
    testcaseid.verify(mozContext.instance.isInitialized())
    mozContext.dumpTS("test_1contextPrepareViewContext end")
}

function shared_2viewInit()
{
    mozContext.dumpTS("test_2viewInit start")
    testcaseid.verify(wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
    testcaseid.verify(mozContext.instance.isInitialized())
    appWindow.createParentID = 0;
    MyScript.createSpriteObjects();
    testcaseid.verify(wrtWait(function() { return (mozView === undefined); }))
    testcaseid.verify(wrtWait(function() { return (mozViewInitialized !== true); }))
    testcaseid.verify(mozView !== undefined)
    mozContext.dumpTS("test_2viewInit end")
}
function shared_3viewLoadURL()
{
    mozContext.dumpTS("test_3viewLoadURL start")
    testcaseid.verify(mozView !== undefined)
    mozView.url = "about:mozilla";
    testcaseid.verify(MyScript.waitLoadFinished(mozView))
    testcaseid.verify(wrtWait(function() { return (mozView.url === "about:mozilla"); }))
    testcaseid.verify(wrtWait(function() { return (!mozView.painted); }))
    mozContext.dumpTS("test_3viewLoadURL end")
}

function shared_TestFaviconPage()
{
    mozContext.dumpTS("test_TestFaviconPage start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/favicons/favicon.html";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.verify(wrtWait(function() { return (!appWindow.favicon); }))
    testcaseid.compare(appWindow.favicon, "data:image/x-icon;base64,AAABAAEAEBAAAAAAAABoBQAAFgAAACgAAAAQAAAAIAAAAAEACAAAAAAAAAEAAAAAAAAAAAAAAAEAAAAAAAAAAAAADPH1AAwM9QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAQEBAQAAAAAAAAAAAAAAAQICAgEAAAAAAAAAAAAAAQECAgIBAAAAAAAAAAAAAQICAgICAQAAAAAAAAAAAAEBAgIBAQEAAAAAAAAAAAEBAQICAQAAAAAAAAAAAAABAgIBAQEAAAAAAAAAAAAAAAEBAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP/gAAD/4AAA58AAAPuAAAC9gAAA/QMAAL0DAAD9jwAA+/8AAOf/AAD//wAAqIAAAKuqAACJqgAAq6oAAKioAAA=");
    mozContext.dumpTS("test_TestFaviconPage");
}

function shared_Test1MultiTouchPage()
{
    mozContext.dumpTS("test_Test1MultiTouchPage start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/multitouch/touch.html";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    var params = [Qt.point(50,50), Qt.point(51,51), Qt.point(52,52)];
    webViewport.synthTouchBegin(params);
    params = [Qt.point(51,51), Qt.point(52,52), Qt.point(53,53)];
    webViewport.synthTouchMove(params);
    params = [Qt.point(52,52), Qt.point(53,53), Qt.point(54,54)];
    webViewport.synthTouchEnd(params);
    webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                        name: "result" })
    testcaseid.verify(wrtWait(function() { return (appWindow.testResult == ""); }))
    testcaseid.compare(appWindow.testResult, "ok");
    mozContext.dumpTS("test_Test1MultiTouchPage end");
}

function shared_1newcontextPrepareViewContext()
{
    mozContext.dumpTS("test_1newcontextPrepareViewContext start")
    testcaseid.verify(mozContext.instance !== undefined)
    testcaseid.verify(wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
    testcaseid.verify(mozContext.instance.isInitialized())
    mozContext.dumpTS("test_1newcontextPrepareViewContext end")
}
function shared_2newviewInit()
{
    mozContext.dumpTS("test_2newviewInit start")
    testcaseid.verify(wrtWait(function() { return (mozContext.instance.isInitialized() === false); }, 100, 500))
    testcaseid.verify(mozContext.instance.isInitialized())
    MyScript.createSpriteObjects();
    testcaseid.verify(wrtWait(function() { return (mozView === null); }, 10, 500))
    testcaseid.verify(wrtWait(function() { return (mozViewInitialized !== true); }, 10, 500))
    testcaseid.verify(mozView !== undefined)
    mozContext.dumpTS("test_2newviewInit end")
}
function shared_viewTestNewWindowAPI()
{
    mozContext.dumpTS("test_viewTestNewWindowAPI start")
    testcaseid.verify(wrtWait(function() { return (mozView === undefined); }, 100, 500))
    testcaseid.verify(mozView !== undefined)
    mozView.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/newviewrequest/newwin.html";
    testcaseid.verify(MyScript.waitLoadFinished(mozView))
    testcaseid.compare(mozView.title, "NewWinExample")
    testcaseid.verify(wrtWait(function() { return (!mozView.painted); }))
    mozViewInitialized = false;
    testcaseid.mouseClick(mozView, 10, 10)
    testcaseid.verify(wrtWait(function() { return (!mozView || !oldMozView); }))
    testcaseid.verify(wrtWait(function() { return (mozViewInitialized !== true); }))
    testcaseid.verify(mozView !== undefined)
    testcaseid.verify(MyScript.waitLoadFinished(mozView))
    testcaseid.verify(wrtWait(function() { return (!mozView.painted); }))
    testcaseid.compare(mozView.url, "about:mozilla")
    mozContext.dumpTS("test_viewTestNewWindowAPI end")
}

function shared_TestLoginMgrPage()
{
    mozContext.dumpTS("test_TestLoginMgrPage start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    appWindow.promptReceived = false
    webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/passwordmgr/subtst_notifications_1.html";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.verify(wrtWait(function() { return (!appWindow.promptReceived); }))
    mozContext.dumpTS("test_TestLoginMgrPage end");
}

function shared_TestPromptPage()
{
    mozContext.dumpTS("test_TestPromptPage start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    appWindow.testCaseNum = 0
    appWindow.promptReceived = false
    appWindow.testResult = null
    webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.verify(wrtWait(function() { return (!appWindow.promptReceived); }))
    webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                        name: "result" })
    testcaseid.verify(wrtWait(function() { return (!appWindow.testResult); }))
    testcaseid.compare(appWindow.testResult, "ok");
    mozContext.dumpTS("test_TestPromptPage end");
}

function shared_TestPromptWithBadResponse()
{
    mozContext.dumpTS("test_TestPromptWithBadResponse start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    appWindow.testCaseNum = 1
    appWindow.promptReceived = false
    appWindow.testResult = null
    webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.verify(wrtWait(function() { return (!appWindow.promptReceived); }))
    webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                        name: "result" })
    testcaseid.verify(wrtWait(function() { return (!appWindow.testResult); }))
    testcaseid.compare(appWindow.testResult, "failed");
    mozContext.dumpTS("test_TestPromptWithBadResponse end");
}

function shared_TestPromptWithoutResponse()
{
    mozContext.dumpTS("test_TestPromptWithoutResponse start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    appWindow.testCaseNum = 2
    appWindow.promptReceived = false
    appWindow.testResult = null
    webViewport.url = mozContext.getenv("QTTESTSROOT") + "/auto/shared/promptbasic/prompt.html";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.verify(wrtWait(function() { return (!appWindow.promptReceived); }))
    webViewport.sendAsyncMessage("embedtest:getelementinner", {
                                        name: "result" })
    testcaseid.verify(wrtWait(function() { return (!appWindow.testResult); }))
    testcaseid.compare(appWindow.testResult, "unknown");
    mozContext.dumpTS("test_TestPromptWithoutResponse end");
}

function shared_TestCheckDefaultSearch()
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
    testcaseid.verify(MyScript.waitMozContext())
    mozContext.instance.setPref("browser.search.log", true);
    mozContext.instance.addObserver("browser-search-engine-modified");
    mozContext.instance.addObserver("embed:search");
    mozContext.instance.setPref("keyword.enabled", true);
    testcaseid.verify(MyScript.waitMozView())
    mozContext.instance.sendObserve("embedui:search", {msg:"remove", name: "QMOZTest"})
    testcaseid.verify(wrtWait(function() { return (!engineExistsPredicate()); }))
    mozContext.instance.sendObserve("embedui:search", {msg:"loadxml", uri: "file://" + mozContext.getenv("QTTESTSROOT") + "/auto/shared/searchengine/test.xml", confirm: false})
    testcaseid.verify(wrtWait(function() { return (appWindow.testResult !== "loaded"); }))
    mozContext.instance.sendObserve("embedui:search", {msg:"getlist"})
    testcaseid.verify(wrtWait(engineExistsPredicate));
    webViewport.load("linux home");
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.compare(webViewport.url.toString().substr(0, 34), "https://webhook/?search=linux+home")
    mozContext.dumpTS("TestCheckDefaultSearch end");
}

function shared_Test1LoadSimpleBlank()
{
    mozContext.dumpTS("test_Test1LoadSimpleBlank start")
    testcaseid.verify(MyScript.waitMozContext())
    testcaseid.verify(MyScript.waitMozView())
    webViewport.url = "about:blank";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100)
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    mozContext.dumpTS("test_Test1LoadSimpleBlank end")
}
function shared_Test2LoadAboutMozillaCheckTitle()
{
    mozContext.dumpTS("test_Test2LoadAboutMozillaCheckTitle start")
    webViewport.url = "about:mozilla";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.title, "The Book of Mozilla, 15:1")
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    mozContext.dumpTS("test_Test2LoadAboutMozillaCheckTitle end")
}

function shared_ActiveHyperLink()
{
    mozContext.dumpTS("test_ActiveHyperLink start")
    testcaseid.verify(MyScript.waitMozContext())
    mozContext.instance.sendObserve("embedui:setprefs", { prefs :
    [
        { n: "embedlite.azpc.handle.singletap", v: false},
        { n: "embedlite.azpc.json.singletap", v: true},
        { n: "embedlite.azpc.handle.longtap", v: false},
        { n: "embedlite.azpc.json.longtap", v: true},
        { n: "embedlite.azpc.json.viewport", v: true},
        { n: "browser.ui.touch.left", v: 32},
        { n: "browser.ui.touch.right", v: 32},
        { n: "browser.ui.touch.top", v: 48},
        { n: "browser.ui.touch.bottom", v: 16},
        { n: "browser.ui.touch.weight.visited", v: 120}
    ]});
    testcaseid.verify(MyScript.waitMozView())
    webViewport.url = "data:text/html,<head><meta name='viewport' content='initial-scale=1'></head><body><a href=about:mozilla>ActiveLink</a>";
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    testcaseid.mouseClick(webViewport, 10, 20)
    testcaseid.verify(wrtWait(function() {
        return webViewport.url != "about:mozilla";
    }))
    testcaseid.verify(MyScript.waitLoadFinished(webViewport))
    testcaseid.compare(webViewport.loadProgress, 100);
    testcaseid.verify(wrtWait(function() { return (!webViewport.painted); }))
    mozContext.instance.sendObserve("embedui:setprefs", { prefs :
    [
        { n: "embedlite.azpc.handle.singletap", v: true},
        { n: "embedlite.azpc.json.singletap", v: false},
        { n: "embedlite.azpc.handle.longtap", v: true},
        { n: "embedlite.azpc.json.longtap", v: false},
        { n: "embedlite.azpc.json.viewport", v: false},
        { n: "browser.ui.touch.left", v: 32},
        { n: "browser.ui.touch.right", v: 32},
        { n: "browser.ui.touch.top", v: 48},
        { n: "browser.ui.touch.bottom", v: 16},
        { n: "browser.ui.touch.weight.visited", v: 120}
    ]});
    mozContext.dumpTS("test_ActiveHyperLink end")
}
