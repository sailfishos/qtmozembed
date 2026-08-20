/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITEWINDOW_H
#define TEST_EMBEDLITEWINDOW_H

#include "EmbedLiteChromeContentSession.h"
#include "EmbedLiteChromeSession.h"
#include "EmbedLiteChromeInputSession.h"
#include "EmbedLiteChromeTabSession.h"
#include "EmbedInputData.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct nsIntRect
{
};

namespace mozilla {
namespace embedlite {

enum ScreenRotation {
    ROTATION_0,
    ROTATION_90,
    ROTATION_180,
    ROTATION_270
};

enum class PlatformImageHandleType : uint8_t {
    EGLImage
};

enum class PlatformImageTextureTarget : uint8_t {
    Texture2D,
    ExternalOES
};

struct PlatformImageDescriptor final
{
    PlatformImageHandleType handleType;
    void *handle;
    PlatformImageTextureTarget textureTarget;
    int32_t width;
    int32_t height;
};

using PlatformImageCallback =
        std::function<void(const PlatformImageDescriptor &)>;

struct PlatformFrameToken final
{
    uint64_t epoch;
    uint64_t sequence;

    bool IsValid() const
    {
        return epoch != 0 && sequence != 0;
    }
};

enum class PlatformFrameFenceHandleType : uint8_t {
    NoHandle,
    EGLSync
};

struct PlatformFrameDescriptor final
{
    PlatformFrameToken token;
    PlatformImageDescriptor image;
    PlatformFrameFenceHandleType releaseFenceHandleType;
};

struct PlatformFrameRelease final
{
    PlatformFrameToken token;
    PlatformFrameFenceHandleType fenceHandleType;
    void *fenceHandle;
};

using PlatformFrameCallback =
        std::function<bool(const PlatformFrameDescriptor &)>;

class EmbedLitePlatformFrameListener
{
public:
    virtual void PlatformFrameReady(const PlatformFrameToken &token) = 0;
    virtual void PlatformFrameDeliveryStopped() = 0;

protected:
    virtual ~EmbedLitePlatformFrameListener() = default;
};

class EmbedLiteWindowListener
{
public:
    virtual void WindowInitialized() {}
    virtual void WindowDestroyed() {}
    virtual void CompositorCreated() {}
    virtual void CompositingFinished() {}
    virtual void DrawOverlay(const nsIntRect &) {}
    virtual bool PreRender() { return true; }
    virtual ~EmbedLiteWindowListener() = default;
};

class EmbedLiteChromeWindowListener
{
public:
    virtual void ChromeWindowInitializationFailed() = 0;

protected:
    virtual ~EmbedLiteChromeWindowListener() = default;
};

class TestChromeSession final : public EmbedLiteChromeSession
{
public:
    explicit TestChromeSession(std::vector<std::string> *events)
        : mEvents(events)
        , mListener(nullptr)
    {
    }

    void SetListener(EmbedLiteChromeSessionListener *listener) override
    {
        mListener = listener;
        mEvents->push_back(listener ? "session-listener-set"
                                    : "session-listener-cleared");
    }

    bool LoadURL(const char *url, bool fromExternal) override
    {
        lastURL = url ? url : "";
        lastFromExternal = fromExternal;
        mEvents->push_back("session-load");
        return true;
    }

    bool GoBack(bool, bool) override
    {
        mEvents->push_back("session-back");
        return true;
    }

    bool GoForward(bool, bool) override
    {
        mEvents->push_back("session-forward");
        return true;
    }

    bool StopLoad() override
    {
        mEvents->push_back("session-stop");
        return true;
    }

    bool Reload(bool hard) override
    {
        lastHardReload = hard;
        mEvents->push_back("session-reload");
        return true;
    }

    bool SetActive(bool active) override
    {
        lastActive = active;
        mEvents->push_back("session-active");
        return true;
    }

    bool SetFocused(bool focused) override
    {
        lastFocused = focused;
        mEvents->push_back("session-focused");
        return true;
    }

    bool ReceiveInputEvent(const EmbedTouchInput &event) override
    {
        lastTouchType = event.type;
        lastTouchTimeStamp = event.timeStamp;
        lastTouchCount = event.touches.size();
        if (!event.touches.empty()) {
            lastTouchIdentifier = event.touches.front().identifier;
            lastTouchX = event.touches.front().touchPoint.x;
            lastTouchY = event.touches.front().touchPoint.y;
            lastTouchPressure = event.touches.front().pressure;
        }
        mEvents->push_back("session-touch");
        return true;
    }

    void NotifyState()
    {
        if (mListener) {
            mListener->OnLocationChanged("https://example.com/state",
                                         true, false);
            mListener->OnLoadStarted("https://example.com/state");
            mListener->OnLoadProgress(42, 4, 10);
            mListener->OnTitleChanged(u"Example title");
            mListener->OnLoadFinished();
        }
    }

    void NotifyDestroyed()
    {
        EmbedLiteChromeSessionListener * const listener = mListener;
        mListener = nullptr;
        if (listener) {
            listener->ChromeSessionDestroyed();
        }
    }

    std::string lastURL;
    bool lastFromExternal = false;
    bool lastHardReload = false;
    bool lastActive = false;
    bool lastFocused = false;
    EmbedTouchInput::EmbedTouchType lastTouchType =
            EmbedTouchInput::MULTITOUCH_SENTINEL;
    uint32_t lastTouchTimeStamp = 0;
    std::size_t lastTouchCount = 0;
    int32_t lastTouchIdentifier = 0;
    float lastTouchX = 0.0f;
    float lastTouchY = 0.0f;
    float lastTouchPressure = 0.0f;

private:
    std::vector<std::string> *mEvents;
    EmbedLiteChromeSessionListener *mListener;
};

class TestChromeTabSession final : public EmbedLiteChromeTabSession
{
public:
    explicit TestChromeTabSession(std::vector<std::string> *events)
        : mEvents(events)
        , mListener(nullptr)
    {
    }

    void SetTabListener(
            EmbedLiteChromeTabSessionListener *listener) override
    {
        mListener = listener;
        mEvents->push_back(listener ? "tab-listener-set"
                                    : "tab-listener-cleared");
    }

    bool RestoreTabs(const EmbedLiteChromeRestoredTab *tabs,
                     uint32_t tabCount, int32_t selectedTabIndex) override
    {
        lastRestoreCount = tabCount;
        lastRestoreSelectedIndex = selectedTabIndex;
        lastRestorePersistentId = tabCount ? tabs[0].persistentId : 0;
        lastRestoreLocation = tabCount && tabs[0].historyCount
                ? tabs[0].history[0].location : "";
        return true;
    }

    bool NewTab(const char *url, uint64_t persistentId,
                bool fromExternal, bool inBackground) override
    {
        lastURL = url ? url : "";
        lastPersistentId = persistentId;
        lastFromExternal = fromExternal;
        lastInBackground = inBackground;
        return true;
    }

    bool AssociateTab(uint64_t tabId, uint64_t persistentId) override
    {
        lastTabId = tabId;
        lastPersistentId = persistentId;
        return true;
    }

    bool SelectTab(uint64_t tabId) override
    {
        lastTabId = tabId;
        return true;
    }

    bool CloseTab(uint64_t tabId) override
    {
        lastTabId = tabId;
        return true;
    }

    bool ResolveBeforeUnloadPrompt(
            uint64_t requestId, uint64_t tabId, bool permit) override
    {
        lastBeforeUnloadRequestId = requestId;
        lastBeforeUnloadTabId = tabId;
        lastBeforeUnloadPermit = permit;
        return true;
    }

    void NotifyTabs()
    {
        if (!mListener) {
            return;
        }
        const EmbedLiteChromeTabSnapshot tab = {
            42, 77, 9, "https://example.com/tab", u"Tab title",
            true, false, false, true, false, 25, 1, 4
        };
        mListener->OnTabsChanged(3, tab.id, &tab, 1);
    }

    void NotifyInvalidTabs()
    {
        if (!mListener) {
            return;
        }
        mListener->OnTabsChanged(4, 42, nullptr, 1);

        const EmbedLiteChromeTabSnapshot tabs[] = {
            { 42, 77, 9, "https://example.com/one", u"One",
              false, false, false, false, false, 100, 1, 1 },
            { 42, 78, 10, "https://example.com/two", u"Two",
              false, false, false, false, false, 100, 1, 1 }
        };
        mListener->OnTabsChanged(5, 42, tabs, 2);
    }

    void NotifyBeforeUnloadPrompt()
    {
        if (!mListener) {
            return;
        }
        const EmbedLiteChromeBeforeUnloadPrompt prompt = {
            12345678901234567ULL, 42, 77, u"Leave this page?",
            u"Changes you made may not be saved.", u"Leave", u"Stay"
        };
        mListener->OnBeforeUnloadPrompt(prompt);
    }

    void NotifyDestroyed()
    {
        EmbedLiteChromeTabSessionListener * const listener = mListener;
        mListener = nullptr;
        if (listener) {
            listener->ChromeTabSessionDestroyed();
        }
    }

    uint32_t lastRestoreCount = 0;
    int32_t lastRestoreSelectedIndex = -1;
    uint64_t lastRestorePersistentId = 0;
    std::string lastRestoreLocation;
    std::string lastURL;
    uint64_t lastTabId = 0;
    uint64_t lastPersistentId = 0;
    uint64_t lastBeforeUnloadRequestId = 0;
    uint64_t lastBeforeUnloadTabId = 0;
    bool lastBeforeUnloadPermit = false;
    bool lastFromExternal = false;
    bool lastInBackground = false;

private:
    std::vector<std::string> *mEvents;
    EmbedLiteChromeTabSessionListener *mListener;
};

class TestChromeInputSession final : public EmbedLiteChromeInputSession
{
public:
    explicit TestChromeInputSession(std::vector<std::string> *events)
        : mEvents(events)
        , mListener(nullptr)
    {
    }

    void SetInputListener(
            EmbedLiteChromeInputSessionListener *listener) override
    {
        mListener = listener;
        mEvents->push_back(listener ? "input-listener-set"
                                    : "input-listener-cleared");
    }

    bool SendTextEvent(
            const char *commit, const char *preedit,
            int32_t replacementStart, int32_t replacementLength) override
    {
        lastCommit = commit ? commit : "";
        lastPreedit = preedit ? preedit : "";
        lastReplacementStart = replacementStart;
        lastReplacementLength = replacementLength;
        return true;
    }

    bool SendKeyPress(
            int32_t domKeyCode, int32_t modifiers,
            int32_t charCode) override
    {
        lastPressDomKeyCode = domKeyCode;
        lastPressModifiers = modifiers;
        lastPressCharCode = charCode;
        return true;
    }

    bool SendKeyRelease(
            int32_t domKeyCode, int32_t modifiers,
            int32_t charCode) override
    {
        lastReleaseDomKeyCode = domKeyCode;
        lastReleaseModifiers = modifiers;
        lastReleaseCharCode = charCode;
        return true;
    }

    void NotifyInputContext(
            const char16_t *inputType, const char16_t *inputMode,
            const char16_t *actionHint)
    {
        if (!mListener) {
            return;
        }
        mListener->OnInputContextChanged(
                2, 1, inputType, inputMode, actionHint, 3, 4);
    }

    void NotifyDestroyed()
    {
        EmbedLiteChromeInputSessionListener * const listener = mListener;
        mListener = nullptr;
        if (listener) {
            listener->ChromeInputSessionDestroyed();
        }
    }

    std::string lastCommit;
    std::string lastPreedit;
    int32_t lastReplacementStart = 0;
    int32_t lastReplacementLength = 0;
    int32_t lastPressDomKeyCode = 0;
    int32_t lastPressModifiers = 0;
    int32_t lastPressCharCode = 0;
    int32_t lastReleaseDomKeyCode = 0;
    int32_t lastReleaseModifiers = 0;
    int32_t lastReleaseCharCode = 0;

private:
    std::vector<std::string> *mEvents;
    EmbedLiteChromeInputSessionListener *mListener;
};

class TestChromeContentSession final : public EmbedLiteChromeContentSession
{
public:
    explicit TestChromeContentSession(std::vector<std::string> *events)
        : mEvents(events)
        , mListener(nullptr)
    {
    }

    void SetContentListener(
            EmbedLiteChromeContentSessionListener *listener) override
    {
        mListener = listener;
        mEvents->push_back(listener ? "content-listener-set"
                                    : "content-listener-cleared");
    }

    bool LoadFrameScript(const char *uri) override
    {
        ++loadFrameScriptCallCount;
        lastFrameScript = uri ? uri : "";
        if (failNextLoadFrameScript) {
            failNextLoadFrameScript = false;
            return false;
        }
        return true;
    }

    bool AddMessageListener(const char *name) override
    {
        ++addMessageListenerCallCount;
        lastAddedListener = name ? name : "";
        if (failNextAddMessageListener) {
            failNextAddMessageListener = false;
            return false;
        }
        return true;
    }

    bool RemoveMessageListener(const char *name) override
    {
        lastRemovedListener = name ? name : "";
        return true;
    }

    bool SendAsyncMessage(uint64_t tabId, const char16_t *name,
                          const char16_t *json) override
    {
        lastTabId = tabId;
        lastMessageName = name ? name : u"";
        lastMessageJson = json ? json : u"";
        return true;
    }

    bool SendMouseEvent(
            uint64_t tabId, EmbedLiteChromeMouseType type,
            int32_t x, int32_t y, uint64_t time, uint32_t button,
            uint32_t buttons, uint32_t modifiers,
            uint32_t clickCount) override
    {
        lastTabId = tabId;
        lastMouseType = type;
        lastX = x;
        lastY = y;
        lastTime = time;
        lastButton = button;
        lastButtons = buttons;
        lastModifiers = modifiers;
        lastClickCount = clickCount;
        return true;
    }

    bool SendWheelEvent(
            uint64_t tabId, int32_t x, int32_t y, uint64_t time,
            double deltaX, double deltaY, uint32_t deltaMode,
            uint32_t modifiers) override
    {
        lastTabId = tabId;
        lastX = x;
        lastY = y;
        lastTime = time;
        lastDeltaX = deltaX;
        lastDeltaY = deltaY;
        lastDeltaMode = deltaMode;
        lastModifiers = modifiers;
        return true;
    }

    bool ScrollTo(uint64_t tabId, int32_t x, int32_t y) override
    {
        lastTabId = tabId;
        lastX = x;
        lastY = y;
        lastRelativeScroll = false;
        return true;
    }

    bool ScrollBy(uint64_t tabId, int32_t x, int32_t y) override
    {
        lastTabId = tabId;
        lastX = x;
        lastY = y;
        lastRelativeScroll = true;
        return true;
    }

    bool ZoomToRect(uint64_t tabId, float x, float y,
                    float width, float height) override
    {
        lastTabId = tabId;
        lastZoomX = x;
        lastZoomY = y;
        lastZoomWidth = width;
        lastZoomHeight = height;
        return true;
    }

    bool SetDesktopMode(uint64_t tabId, bool desktopMode) override
    {
        lastTabId = tabId;
        lastDesktopMode = desktopMode;
        return true;
    }

    bool SetThrottlePainting(uint64_t tabId, bool throttle) override
    {
        lastTabId = tabId;
        lastThrottlePainting = throttle;
        return true;
    }

    bool SuspendTimeouts(uint64_t tabId) override
    {
        lastTabId = tabId;
        lastTimeoutsSuspended = true;
        return true;
    }

    bool ResumeTimeouts(uint64_t tabId) override
    {
        lastTabId = tabId;
        lastTimeoutsSuspended = false;
        return true;
    }

    bool SetHttpUserAgent(
            uint64_t tabId, const char16_t *httpUserAgent) override
    {
        lastTabId = tabId;
        lastHttpUserAgent = httpUserAgent ? httpUserAgent : u"";
        return true;
    }

    bool SetMargins(uint64_t tabId, int32_t top, int32_t right,
                    int32_t bottom, int32_t left) override
    {
        lastTabId = tabId;
        lastTop = top;
        lastRight = right;
        lastBottom = bottom;
        lastLeft = left;
        return true;
    }

    bool SetSafeAreaInsets(uint64_t tabId, int32_t top, int32_t right,
                           int32_t bottom, int32_t left) override
    {
        lastTabId = tabId;
        lastSafeTop = top;
        lastSafeRight = right;
        lastSafeBottom = bottom;
        lastSafeLeft = left;
        return true;
    }

    bool SetDynamicToolbarHeight(uint64_t tabId, int32_t height) override
    {
        lastTabId = tabId;
        lastDynamicToolbarHeight = height;
        return true;
    }

    bool SetScreenProperties(
            int32_t depth, float density, float dpi) override
    {
        lastDepth = depth;
        lastDensity = density;
        lastDpi = dpi;
        return true;
    }

    void NotifyState(const char *securityStatus)
    {
        if (!mListener) {
            return;
        }
        const EmbedLiteChromeContentState state = {
            42, 77, 13, 11, securityStatus, 5, true, true, 6, 7,
            1200, 2400, 30, 40, 30.5, 40.5, 360.0, 640.0
        };
        mListener->OnContentStateChanged(state);
    }

    void NotifyAsyncMessage(const char16_t *name, const char16_t *json)
    {
        if (mListener) {
            mListener->RecvAsyncMessage(42, 77, 11, name, json);
        }
    }

    void NotifyWindowCloseRequested()
    {
        if (mListener) {
            mListener->OnWindowCloseRequested(42, 77);
        }
    }

    void NotifyCloseResult(uint64_t tabId, bool closed)
    {
        if (mListener) {
            mListener->OnTabCloseResult(tabId, closed);
        }
    }

    void NotifyDestroyed()
    {
        EmbedLiteChromeContentSessionListener * const listener = mListener;
        mListener = nullptr;
        if (listener) {
            listener->ChromeContentSessionDestroyed();
        }
    }

    uint64_t lastTabId = 0;
    bool failNextLoadFrameScript = false;
    bool failNextAddMessageListener = false;
    int loadFrameScriptCallCount = 0;
    int addMessageListenerCallCount = 0;
    std::string lastFrameScript;
    std::string lastAddedListener;
    std::string lastRemovedListener;
    std::u16string lastMessageName;
    std::u16string lastMessageJson;
    EmbedLiteChromeMouseType lastMouseType =
            EmbedLiteChromeMouseType::Move;
    int32_t lastX = 0;
    int32_t lastY = 0;
    uint64_t lastTime = 0;
    uint32_t lastButton = 0;
    uint32_t lastButtons = 0;
    uint32_t lastModifiers = 0;
    uint32_t lastClickCount = 0;
    double lastDeltaX = 0.0;
    double lastDeltaY = 0.0;
    uint32_t lastDeltaMode = 0;
    bool lastRelativeScroll = false;
    float lastZoomX = 0.0f;
    float lastZoomY = 0.0f;
    float lastZoomWidth = 0.0f;
    float lastZoomHeight = 0.0f;
    bool lastDesktopMode = false;
    bool lastThrottlePainting = false;
    bool lastTimeoutsSuspended = false;
    std::u16string lastHttpUserAgent;
    int32_t lastTop = 0;
    int32_t lastRight = 0;
    int32_t lastBottom = 0;
    int32_t lastLeft = 0;
    int32_t lastSafeTop = 0;
    int32_t lastSafeRight = 0;
    int32_t lastSafeBottom = 0;
    int32_t lastSafeLeft = 0;
    int32_t lastDynamicToolbarHeight = 0;
    int32_t lastDepth = 0;
    float lastDensity = 0.0f;
    float lastDpi = 0.0f;

private:
    std::vector<std::string> *mEvents;
    EmbedLiteChromeContentSessionListener *mListener;
};

class EmbedLiteWindow
{
public:
    explicit EmbedLiteWindow(std::vector<std::string> *events)
        : mEvents(events)
        , mChromeSession(events)
        , mChromeTabSession(events)
        , mChromeInputSession(events)
        , mChromeContentSession(events)
        , mUniqueID(nextUniqueID())
        , mChromeHosted(false)
        , mFrameListener(nullptr)
        , mDeliveryEnabled(false)
        , mAcquired(false)
        , mReadyToken({ 0, 0 })
        , mAcquiredToken({ 0, 0 })
    {
    }

    void SetSize(int, int) {}
    void SetContentOrientation(ScreenRotation) {}
    void ScheduleUpdate() {}
    void SuspendRendering() {}
    void ResumeRendering() {}
    void ClearPlatformImage() {}

    void SetChromeHosted(bool hosted) { mChromeHosted = hosted; }
    uint32_t GetUniqueID() const { return mUniqueID; }
    EmbedLiteChromeSession *GetChromeSession()
    {
        return mChromeHosted ? &mChromeSession : nullptr;
    }
    TestChromeSession &ChromeSession() { return mChromeSession; }
    EmbedLiteChromeTabSession *GetChromeTabSession()
    {
        return mChromeHosted ? &mChromeTabSession : nullptr;
    }
    TestChromeTabSession &ChromeTabSession() { return mChromeTabSession; }
    EmbedLiteChromeInputSession *GetChromeInputSession()
    {
        return mChromeHosted ? &mChromeInputSession : nullptr;
    }
    TestChromeInputSession &ChromeInputSession()
    {
        return mChromeInputSession;
    }
    EmbedLiteChromeContentSession *GetChromeContentSession()
    {
        return mChromeHosted ? &mChromeContentSession : nullptr;
    }
    TestChromeContentSession &ChromeContentSession()
    {
        return mChromeContentSession;
    }

    bool WithPlatformImage(const PlatformImageCallback &)
    {
        return false;
    }

    bool SetPlatformFrameListener(
            EmbedLitePlatformFrameListener *listener)
    {
        mFrameListener = listener;
        mEvents->push_back(listener ? "listener-set" : "listener-cleared");
        return true;
    }

    bool SetPlatformFrameDeliveryEnabled(bool enabled)
    {
        if (!enabled && mAcquired) {
            mEvents->push_back("disable-blocked");
            return false;
        }
        mDeliveryEnabled = enabled;
        mEvents->push_back(enabled ? "enabled" : "disabled");
        return true;
    }

    bool AcquirePlatformFrame(const PlatformFrameToken &token,
                              const PlatformFrameCallback &callback)
    {
        if (!mDeliveryEnabled || mAcquired || !callback
                || token.epoch != mReadyToken.epoch
                || token.sequence != mReadyToken.sequence) {
            return false;
        }

        const bool accepted = callback({
            token,
            { PlatformImageHandleType::EGLImage,
              reinterpret_cast<void *>(2),
              PlatformImageTextureTarget::ExternalOES, 2, 3 },
            PlatformFrameFenceHandleType::EGLSync
        });
        if (accepted) {
            mAcquired = true;
            mAcquiredToken = token;
            mEvents->push_back("acquired");
        }
        return accepted;
    }

    bool ReleasePlatformFrame(const PlatformFrameRelease &release)
    {
        if (!mAcquired || release.token.epoch != mAcquiredToken.epoch
                || release.token.sequence != mAcquiredToken.sequence) {
            return false;
        }
        mAcquired = false;
        mEvents->push_back("released");
        return true;
    }

    void NotifyFrameReady(const PlatformFrameToken &token)
    {
        mReadyToken = token;
        if (mDeliveryEnabled && mFrameListener) {
            mFrameListener->PlatformFrameReady(token);
        }
    }

    void NotifyFrameDeliveryStopped()
    {
        if (mFrameListener) {
            mEvents->push_back("stopped-notified");
            mFrameListener->PlatformFrameDeliveryStopped();
        }
    }

private:
    static uint32_t nextUniqueID()
    {
        static uint32_t id = 0;
        return ++id;
    }

    std::vector<std::string> *mEvents;
    TestChromeSession mChromeSession;
    TestChromeTabSession mChromeTabSession;
    TestChromeInputSession mChromeInputSession;
    TestChromeContentSession mChromeContentSession;
    uint32_t mUniqueID;
    bool mChromeHosted;
    EmbedLitePlatformFrameListener *mFrameListener;
    bool mDeliveryEnabled;
    bool mAcquired;
    PlatformFrameToken mReadyToken;
    PlatformFrameToken mAcquiredToken;
};

} // namespace embedlite
} // namespace mozilla

#endif // TEST_EMBEDLITEWINDOW_H
