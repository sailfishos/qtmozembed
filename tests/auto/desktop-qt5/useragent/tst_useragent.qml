import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0

Item {
    id: appWindow
    width: 480
    height: 800

    readonly property string override_default: "Default domain-specific override"
    readonly property string override_developer: "Developer specified override"
    readonly property string test_page: "https://browser.sailfishos.org/tests/testuseragent.html"

    QmlMozView {
        id: webViewport
        visible: true
        focus: true
        active: true
        anchors.fill: parent
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
            QMozEngineSettings.setPreference("general.useragent.override.sailfishos.org", override_default)

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            webViewportSpy.clear()
        }

        function test_TestUserAgentDefaultPage() {
            // Check empty httpUserAgent string gets set to actual
            webViewport.httpUserAgent = ""
            verify(webViewport.httpUserAgent === "")
            loadPage(test_page)
            verify(webViewport.httpUserAgent === override_default)
        }

        function test_TestUserAgentDeveloperPage() {
            // Check non-empty httpUserAgent string is not changed
            webViewport.httpUserAgent = override_developer
            verify(webViewport.httpUserAgent === override_developer)
            loadPage(test_page)
            verify(webViewport.httpUserAgent === override_developer)
        }

        function test_TestUserAgentClearPage() {
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
