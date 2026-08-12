/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_EMBEDLITEWINDOW_H
#define TEST_EMBEDLITEWINDOW_H

#include "EmbedLiteChromeSession.h"

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

private:
    std::vector<std::string> *mEvents;
    EmbedLiteChromeSessionListener *mListener;
};

class EmbedLiteWindow
{
public:
    explicit EmbedLiteWindow(std::vector<std::string> *events)
        : mEvents(events)
        , mChromeSession(events)
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
    EmbedLiteChromeSession *GetChromeSession()
    {
        return mChromeHosted ? &mChromeSession : nullptr;
    }
    TestChromeSession &ChromeSession() { return mChromeSession; }

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
    std::vector<std::string> *mEvents;
    TestChromeSession mChromeSession;
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
