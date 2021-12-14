import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript
import "../../shared"

TestWindow {
    id: appWindow

    readonly property string override_default: "Default domain-specific override"
    readonly property string override_developer: "Developer specified override"
    readonly property string test_page: "https://browser.sailfishos.org/tests/testuseragent.html"

    name: testcaseid.name

    Connections {
        target: QmlMozContext
        onOnInitialized: {
            QmlMozContext.addComponentManifest(TestHelper.getenv("QTTESTSROOT") + "/components/TestHelpers.manifest")
        }
    }

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
        property string useragent: ""

        Component.onCompleted: {
            webViewport.loadFrameScript("chrome://tests/content/testHelper.js")
            webViewport.addMessageListener("testembed:useragent")
        }

        onRecvAsyncMessage: {
            switch (message) {
            case "testembed:useragent": {
                webViewport.useragent = data.value
                break;
                }
            }
        }
    }

    SignalSpy {
        id: webViewportSpy
        target: webViewport
        signalName: "viewInitialized"
    }

    function loadPage(url) {
        // Wait for any existing load to complete
        webViewportSpy.signalName = "onLoadingChanged"
        while (webViewport.loading) {
            webViewportSpy.wait()
        }
        testcaseid.compare(webViewport.loading, false)

        // Load the new page
        webViewportSpy.clear()
        webViewport.url = url;
        while ((webViewportSpy.count == 0) || webViewport.loading) {
            webViewportSpy.wait()
        }
        testcaseid.compare(webViewport.loading, false)
        testcaseid.compare(webViewport.loadProgress, 100);
    }

    TestCase {
        id: testcaseid
        name: "tst_useragent"
        when: windowShown

        function initTestCase() {
            QMozEngineSettings.setPreference("general.useragent.override", override_default)

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            webViewportSpy.clear()
        }

        function test_TestUserAgentDefaultPage() {
            // Check empty httpUserAgent string gets set to actual
            webViewport.useragent = ""
            verify(webViewport.useragent === "")
            loadPage(test_page)
            webViewport.sendAsyncMessage("embedtest:useragent", {})
            verify(MyScript.wrtWait(function() { return (webViewport.useragent === ""); }))
            verify(webViewport.useragent === override_default)
        }

        function test_TestUserAgentDeveloperPage() {
            skip("QuickMozView::httpUserAgent is always empty with current esr78 engine, please see JB#56714")

            // Check non-empty httpUserAgent string is not changed
            webViewport.httpUserAgent = override_developer
            verify(webViewport.httpUserAgent === override_developer)
            loadPage(test_page)
            verify(webViewport.httpUserAgent === override_developer)
        }

        function test_TestUserAgentClearPage() {
            skip("QuickMozView::httpUserAgent is always empty with current esr78 engine, please see JB#56714")

            // Check non-empty httpUserAgent string is not changed
            webViewport.httpUserAgent = override_developer
            verify(webViewport.httpUserAgent === override_developer)
            loadPage(test_page)
            verify(webViewport.httpUserAgent === override_developer)

            // Check cleared httpUserAgent will be reset on next load
            webViewport.httpUserAgent = ""
            verify(webViewport.httpUserAgent === "")
            loadPage(test_page)
            verify(webViewport.httpUserAgent === override_default)
        }

        function test_TestUserAgentSignalSent() {
            skip("QuickMozView::httpUserAgent is always empty with current esr78 engine, please see JB#56714")

            // Check signal is sent when the empty httpUserAgent is updated
            webViewport.httpUserAgent = ""
            verify(webViewport.httpUserAgent === "")

            // Load the page
            webViewportSpy.signalName = "httpUserAgentChanged"
            webViewportSpy.clear()
            webViewport.url = test_page;

            // Check the httpUserAgentChanged signal is sent
            webViewportSpy.wait()
            verify(webViewport.httpUserAgent === override_default)

            // Wait for load to complete
            webViewportSpy.signalName = "onLoadingChanged"
            while (webViewport.loading) {
                webViewportSpy.wait()
            }
            compare(webViewport.loading, false)
        }
    }
}
