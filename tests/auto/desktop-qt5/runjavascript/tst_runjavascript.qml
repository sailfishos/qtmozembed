/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2021 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import QtTest 1.0
import QtQuick 2.0
import Qt5Mozilla 1.0
import QtMozEmbed.Tests 1.0
import "../../shared/componentCreation.js" as MyScript

Item {
    id: appWindow
    width: 480
    height: 800

    QmlMozView {
        id: webViewport

        property var result
        property string error

        signal successCallback(var result)
        signal errorCallback(string error)

        focus: true
        active: true
        anchors.fill: parent

        onSuccessCallback: webViewport.result = result
        onErrorCallback: webViewport.error = error
    }

    SignalSpy {
        id: webViewportSpy
        target: webViewport
        signalName: "viewInitialized"
    }

    TestCase {
        id: testcaseid
        name: "tst_runjavascript"
        when: windowShown

        function initTestCase() {
            if (webViewportSpy.count === 0)
                webViewportSpy.wait()

            compare(webViewportSpy.count, 1)

            webViewportSpy.clear()
            webViewportSpy.signalName = "loadedChanged"
            webViewport.url = TestHelper.getenv("QTTESTSROOT") + "/auto/desktop-qt5/runjavascript/tst_runjavascript.html";
            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            webViewportSpy.clear()
        }

        function cleanup() {
            webViewportSpy.clear()
            webViewport.result = ""
            webViewport.error = ""
        }

        function test_1getElementById() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("return document.getElementById('foobar').innerHTML", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, "HelloWorld")
        }

        function test_1notCallableSuccessCallback() {
            webViewportSpy.signalName = "errorCallback"

            webViewport.runJavaScript("return document.getElementById('foobar').innerHTML", 5, function (error) {
                webViewport.errorCallback(error)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.error, "Error: callback argument is not a function.")
        }

        function test_1javascriptExecutionError() {
            webViewportSpy.signalName = "errorCallback"
            webViewport.runJavaScript("return document.getFooElementById('foobar').innerHTML", function() {}, function (error) {
                webViewport.errorCallback(error)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.error, "TypeError: document.getFooElementById is not a function")

        }

        function test_booleanReturnType() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("return booleanType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, false)
        }

        function test_undefinedReturnType() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("return undefinedType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, undefined)
        }

        function test_numberReturnType() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("return numberType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, 5)
        }

        function test_stringReturnType() {
            webViewportSpy.signalName = "successCallback"

            webViewport.runJavaScript("return stringType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, "TestString")
        }

        function test_objectReturnType() {
            webViewportSpy.signalName = "successCallback"

            webViewport.runJavaScript("return objectType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result.testValue, 5)
            compare(webViewport.result.testMemberString, "HelloWorld")
        }

        function test_symbolReturnType() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("return symbolType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, "Symbol(TestSymbol)")
        }

        function test_functionReturnType() {
            webViewportSpy.signalName = "errorCallback"
            webViewport.runJavaScript("return functionType", function (result) {
                webViewport.successCallback(result)
            }, function (error) {
                webViewport.errorCallback(error);
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.error, "Error: cannot return a function.")
        }

        function test_arrayReturnType() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("return arrayType", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result.length, 3)
            compare(webViewport.result[0], 1)
            compare(webViewport.result[1], 2)
            compare(webViewport.result[2], "Test")
        }

        function test_99noCallbacks() {
            webViewportSpy.signalName = "successCallback"
            webViewport.runJavaScript("foobar = document.getElementById('foobar');\n" +
                                      "foobar.innerHTML = 'Edited HelloWorld'");

            // Verify edited innerHTML change.
            webViewport.runJavaScript("return document.getElementById('foobar').innerHTML", function (result) {
                webViewport.successCallback(result)
            })

            webViewportSpy.wait()
            compare(webViewportSpy.count, 1)
            compare(webViewport.result, "Edited HelloWorld")
        }
    }
}
