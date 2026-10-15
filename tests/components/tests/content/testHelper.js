/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

(function() {
if (globalThis.qtmozTestHelper) {
  globalThis.qtmozTestHelper._cancelApz?.();
  for (const name of ["embedtest:getelementprop", "embedtest:getelementinner",
                      "embedtest:focustoelement", "embedtest:clickelement",
                      "embedtest:useragent", "embedtest:flushapz"]) {
    removeMessageListener(name, globalThis.qtmozTestHelper);
  }
}

dump("###################################### TestHelper.js loaded\n");

function TestHelper() {
  this._init();
}

TestHelper.prototype = {
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
    addMessageListener("embedtest:flushapz", this);
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
      case "embedtest:flushapz": {
        this._cancelApz?.();
        const win = content;
        const document = win.document;
        let pending = true;
        let timer;
        const finish = (ready, reply = true) => {
          if (!pending) return;
          pending = false;
          Services.obs.removeObserver(done, "apz-repaints-flushed");
          win.removeEventListener("pagehide", cancel);
          win.clearTimeout(timer);
          this._cancelApz = null;
          if (reply && content.document === document) {
            sendAsyncMessage("testembed:apzready", {value: ready});
          }
        };
        const done = () => finish(true);
        const cancel = () => finish(false, false);
        this._cancelApz = cancel;
        Services.obs.addObserver(done, "apz-repaints-flushed");
        win.addEventListener("pagehide", cancel);
        timer = win.setTimeout(() => finish(false), 5000);
        try {
          // A content paint can precede the compositor's APZ state update.
          if (!win.windowUtils.flushApzRepaints()) done();
        } catch (error) {
          finish(false);
        }
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

globalThis.qtmozTestHelper = new TestHelper();
})();

