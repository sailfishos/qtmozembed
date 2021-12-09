/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyServiceGetter(Services, "embedlite",
                                    "@mozilla.org/embedlite-app-service;1",
                                    "nsIEmbedAppService");

const kStateActive = 0x00000001; // :active pseudoclass for elements

dump("###################################### TestHelper.js loaded\n");

var globalObject = null;

function TestHelper() {
  this._init();
}

TestHelper.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  _fastFind: null,
  _init: function()
  {
    dump("Init Called:" + this + "\n");
    // addEventListener("touchend", this, false);
    // Services.obs.addObserver(this, "before-first-paint", true);
    addMessageListener("embedtest:getelementprop", this);
    addMessageListener("embedtest:getelementinner", this);
    addMessageListener("embedtest:focustoelement", this);
    addMessageListener("embedtest:clickelement", this);
    addMessageListener("embedtest:useragent", this);
  },

  observe: function(aSubject, aTopic, data) {
    // Ignore notifications not about our document.
    dump("observe topic:" + aTopic + "\n");
  },

  receiveMessage: function receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "embedtest:getelementprop": {
        let element = content.document.getElementById(aMessage.json.name);
        sendAsyncMessage("testembed:elementpropvalue", {value: element.value});
        break;
      }
      case "embedtest:getelementinner": {
        let element = content.document.getElementById(aMessage.json.name);
        sendAsyncMessage("testembed:elementinnervalue", {value: element.innerHTML});
        break;
      }
      case "embedtest:focustoelement": {
        let element = content.document.getElementById(aMessage.json.name);
        element.focus();
        break;
      }
      case "embedtest:clickelement": {
        let element = content.document.getElementById(aMessage.json.name);
        element.click();
        break;
      }
      case "embedtest:useragent": {
        sendAsyncMessage("testembed:useragent", {value: content.navigator.userAgent});
        break;
      }
      default: {
        break;
      }
    }
  },
};

globalObject = new TestHelper();

