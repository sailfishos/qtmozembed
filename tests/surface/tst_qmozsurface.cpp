/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "runtime/qmozsurface_p.h"
#include "runtime/qmozframestream_p.h"
#include "runtime/qmozchromehost_p.h"
#include "backends/embedlite/embedlitesurface_p.h"

#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteWindow.h"

#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace mozilla::embedlite;

namespace {

int failures = 0;

void verify(bool condition, const char *expression, int line)
{
    if (!condition) {
        std::fprintf(stderr, "line %d: verification failed: %s\n",
                     line, expression);
        ++failures;
    }
}

#define VERIFY(expression) verify((expression), #expression, __LINE__)

class FakeSurface final : public QMozSurface
{
public:
    FakeSurface(QMozWindow *window, int *destructions)
        : mWindow(window)
        , mDestructions(destructions)
        , mDestroyRequests(0)
        , mFrameDeliveryEnabled(false)
        , mReleasedFrame({ { 0, 0 },
                           QMozSurfaceFrameFenceType::NoHandle, nullptr })
    {
    }

    ~FakeSurface() override
    {
        ++*mDestructions;
    }

    void requestDestroy() override
    {
        ++mDestroyRequests;
        QtMoz::takeWindowSurface(mWindow);
    }

    void backendDestroyed() override
    {
    }

    bool setSize(const QSize &) override
    {
        return true;
    }

    bool setContentOrientation(QMozSurfaceRotation) override
    {
        return true;
    }

    bool withPlatformImage(
            const QMozSurfaceImageCallback &callback) override
    {
        if (!callback) {
            return false;
        }
        callback({ reinterpret_cast<void *>(1), QSize(1, 1),
                   QMozSurfaceTextureTarget::Texture2D });
        return true;
    }

    bool setPlatformFrameCallbacks(
            const QMozSurfaceFrameReadyCallback &readyCallback,
            const QMozSurfaceFrameDeliveryStoppedCallback &stoppedCallback)
            override
    {
        if (mFrameDeliveryEnabled) {
            return false;
        }
        mFrameReadyCallback = readyCallback;
        mFrameDeliveryStoppedCallback = stoppedCallback;
        return true;
    }

    bool setPlatformFrameDeliveryEnabled(bool enabled) override
    {
        if (enabled && !mFrameReadyCallback) {
            return false;
        }
        mFrameDeliveryEnabled = enabled;
        if (!enabled && mFrameDeliveryStoppedCallback) {
            mFrameDeliveryStoppedCallback();
        }
        return true;
    }

    bool acquirePlatformFrame(
            const QMozSurfaceFrameToken &token,
            const QMozSurfaceFrameCallback &callback) override
    {
        if (!mFrameDeliveryEnabled || !token.isValid() || !callback) {
            return false;
        }
        return callback({
            token,
            { reinterpret_cast<void *>(2), QSize(2, 3),
              QMozSurfaceTextureTarget::ExternalOES },
            QMozSurfaceFrameFenceType::EGLSync
        });
    }

    bool releasePlatformFrame(
            const QMozSurfaceFrameRelease &release) override
    {
        if (!release.token.isValid()) {
            return false;
        }
        mReleasedFrame = release;
        return true;
    }

    bool clearPlatformImage() override
    {
        return true;
    }

    bool suspendRendering() override
    {
        return true;
    }

    bool resumeRendering() override
    {
        return true;
    }

    bool scheduleUpdate() override
    {
        return true;
    }

    int destroyRequests() const
    {
        return mDestroyRequests;
    }

    void notifyFrameReady(const QMozSurfaceFrameToken &token)
    {
        if (mFrameDeliveryEnabled && mFrameReadyCallback) {
            mFrameReadyCallback(token);
        }
    }

    const QMozSurfaceFrameRelease &releasedFrame() const
    {
        return mReleasedFrame;
    }

private:
    QMozWindow *mWindow;
    int *mDestructions;
    int mDestroyRequests;
    bool mFrameDeliveryEnabled;
    QMozSurfaceFrameReadyCallback mFrameReadyCallback;
    QMozSurfaceFrameDeliveryStoppedCallback
            mFrameDeliveryStoppedCallback;
    QMozSurfaceFrameRelease mReleasedFrame;
};

class RecordingWindowListener final : public EmbedLiteWindowListener
{
public:
    RecordingWindowListener()
        : initialized(0)
        , destroyed(0)
        , compositorCreated(0)
        , compositingFinished(0)
        , overlays(0)
        , surfaceToClear(nullptr)
    {
    }

    void WindowInitialized() override { ++initialized; }
    void WindowDestroyed() override
    {
        ++destroyed;
        if (surfaceToClear && !surfaceToClear->isNull()) {
            (*surfaceToClear)->backendDestroyed();
            surfaceToClear->clear();
        }
    }
    void CompositorCreated() override { ++compositorCreated; }
    void CompositingFinished() override { ++compositingFinished; }
    void DrawOverlay(const nsIntRect &) override { ++overlays; }
    bool PreRender() override { return false; }

    int initialized;
    int destroyed;
    int compositorCreated;
    int compositingFinished;
    int overlays;
    QSharedPointer<QMozSurface> *surfaceToClear;
};

QMozWindow *fakeWindow(void *storage)
{
    return reinterpret_cast<QMozWindow *>(storage);
}

void processEvents()
{
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents();
}

int eventIndex(const std::vector<std::string> &events,
               const std::string &event)
{
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events.at(i) == event) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void testRegistryLifetime()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    QSharedPointer<QMozSurface> surface(
            new FakeSurface(window, &destructions));

    VERIFY(QtMoz::installWindowSurface(window, surface));
    VERIFY(!QtMoz::installWindowSurface(window, surface));

    QSharedPointer<QMozSurface> snapshot = QtMoz::windowSurface(window);
    VERIFY(snapshot == surface);
    surface.clear();
    VERIFY(destructions == 0);

    QSharedPointer<QMozSurface> removed = QtMoz::takeWindowSurface(window);
    VERIFY(removed == snapshot);
    VERIFY(QtMoz::windowSurface(window).isNull());
    removed.clear();
    VERIFY(destructions == 0);
    snapshot.clear();
    VERIFY(destructions == 1);
}

void testReentrantRemoval()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    QSharedPointer<FakeSurface> surface(
            new FakeSurface(window, &destructions));

    VERIFY(QtMoz::installWindowSurface(window, surface));
    QSharedPointer<QMozSurface> snapshot = QtMoz::windowSurface(window);
    snapshot->requestDestroy();
    VERIFY(surface->destroyRequests() == 1);
    VERIFY(QtMoz::windowSurface(window).isNull());

    surface.clear();
    VERIFY(destructions == 0);
    snapshot.clear();
    VERIFY(destructions == 1);
}

void testPlatformFrameForwarding()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    QSharedPointer<FakeSurface> surface(
            new FakeSurface(window, &destructions));
    QMozSurfaceFrameToken notifiedToken = { 0, 0 };
    bool stopped = false;

    VERIFY(surface->setPlatformFrameCallbacks(
            [&](const QMozSurfaceFrameToken &token) {
        notifiedToken = token;
    }, [&]() {
        stopped = true;
    }));
    VERIFY(surface->setPlatformFrameDeliveryEnabled(true));

    const QMozSurfaceFrameToken token = { 7, 11 };
    surface->notifyFrameReady(token);
    VERIFY(notifiedToken.epoch == token.epoch);
    VERIFY(notifiedToken.sequence == token.sequence);

    bool acquired = false;
    VERIFY(surface->acquirePlatformFrame(
            token, [&](const QMozSurfaceFrame &frame) {
        acquired = true;
        VERIFY(frame.token.epoch == token.epoch);
        VERIFY(frame.token.sequence == token.sequence);
        VERIFY(frame.image.handle == reinterpret_cast<void *>(2));
        VERIFY(frame.image.size == QSize(2, 3));
        VERIFY(frame.image.textureTarget ==
               QMozSurfaceTextureTarget::ExternalOES);
        VERIFY(frame.releaseFenceType ==
               QMozSurfaceFrameFenceType::EGLSync);
        return true;
    }));
    VERIFY(acquired);

    void * const fence = reinterpret_cast<void *>(3);
    VERIFY(surface->releasePlatformFrame({
        token, QMozSurfaceFrameFenceType::EGLSync, fence
    }));
    VERIFY(surface->releasedFrame().token.epoch == token.epoch);
    VERIFY(surface->releasedFrame().token.sequence == token.sequence);
    VERIFY(surface->releasedFrame().fenceType ==
           QMozSurfaceFrameFenceType::EGLSync);
    VERIFY(surface->releasedFrame().fence == fence);

    VERIFY(surface->setPlatformFrameDeliveryEnabled(false));
    VERIFY(stopped);
    surface.clear();
    VERIFY(destructions == 1);
}

void testFrameStreamTracksLatestConsumerFrame()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    QSharedPointer<FakeSurface> surface(
            new FakeSurface(window, &destructions));
    int consumer = 0;
    int updates = 0;

    VERIFY(QtMoz::installWindowFrameStream(window, surface));
    QtMoz::setWindowFrameConsumer(window, &consumer, [&]() {
        ++updates;
    });
    VERIFY(QtMoz::startWindowFrameStream(window));

    const QMozSurfaceFrameToken first = { 5, 7 };
    surface->notifyFrameReady(first);
    VERIFY(updates == 1);

    const QSharedPointer<QtMoz::QMozFrameStream> stream =
            QtMoz::windowFrameStream(window);
    QMozSurfaceFrameToken token = { 0, 0 };
    VERIFY(stream->takePendingFrame(&consumer, &token));
    VERIFY(token.epoch == first.epoch);
    VERIFY(token.sequence == first.sequence);

    stream->restorePendingFrame(&consumer, token);
    const QMozSurfaceFrameToken newer = { 5, 11 };
    surface->notifyFrameReady(newer);
    VERIFY(updates == 2);
    VERIFY(stream->takePendingFrame(&consumer, &token));
    VERIFY(token.epoch == newer.epoch);
    VERIFY(token.sequence == newer.sequence);

    QtMoz::clearWindowFrameConsumer(window, &consumer);
    surface->notifyFrameReady({ 5, 13 });
    VERIFY(updates == 2);
    VERIFY(!stream->takePendingFrame(&consumer, &token));

    const QSharedPointer<QtMoz::QMozFrameStream> removed =
            QtMoz::takeWindowFrameStream(window);
    VERIFY(removed == stream);
    removed->backendDestroyed();
    surface.clear();
    VERIFY(destructions == 0);
}

void testQueuedFrameSuppressedAfterStop()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200));
    int readyCount = 0;
    int stoppedCount = 0;
    bool readyOnOwnerThread = false;
    bool stoppedOnOwnerThread = false;
    QThread * const ownerThread = QThread::currentThread();

    VERIFY(window == &app.window);
    VERIFY(surface->setPlatformFrameCallbacks(
            [&](const QMozSurfaceFrameToken &) {
        ++readyCount;
        readyOnOwnerThread = QThread::currentThread() == ownerThread;
    }, [&]() {
        ++stoppedCount;
        stoppedOnOwnerThread = QThread::currentThread() == ownerThread;
    }));
    VERIFY(surface->setPlatformFrameDeliveryEnabled(true));
    std::thread readyThread([&]() {
        window->NotifyFrameReady({ 3, 4 });
    });
    readyThread.join();
    VERIFY(readyCount == 0);
    processEvents();
    VERIFY(readyCount == 1);
    VERIFY(readyOnOwnerThread);

    std::thread suppressedReadyThread([&]() {
        window->NotifyFrameReady({ 3, 5 });
    });
    suppressedReadyThread.join();
    VERIFY(surface->setPlatformFrameDeliveryEnabled(false));
    std::thread stoppedThread([&]() {
        window->NotifyFrameDeliveryStopped();
    });
    stoppedThread.join();
    VERIFY(stoppedCount == 0);
    processEvents();

    VERIFY(readyCount == 1);
    VERIFY(stoppedCount == 1);
    VERIFY(stoppedOnOwnerThread);
    surface->requestDestroy();
    VERIFY(app.destroyCount == 1);
    surface->backendDestroyed();
    surface.clear();
}

void testChromeWindowSelection()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    const QByteArray initialUrl("https://example.com/chrome-smoke");
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200), initialUrl);

    VERIFY(window == &app.window);
    VERIFY(app.createCount == 0);
    VERIFY(app.chromeCreateCount == 1);
    VERIFY(app.chromeInitialUrl == initialUrl.constData());
    VERIFY(eventIndex(app.events, "chrome-created") >= 0);
    VERIFY(eventIndex(app.events, "listener-set") >= 0);

    surface->requestDestroy();
    VERIFY(app.destroyCount == 1);
    VERIFY(eventIndex(app.events, "listener-cleared") <
           eventIndex(app.events, "destroyed"));
    surface->backendDestroyed();
    surface.clear();
}

void testChromeWindowFailureMarshalledToOwnerThread()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QThread * const ownerThread = QThread::currentThread();
    bool failed = false;
    bool failedOnOwnerThread = false;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(
                &app, &listener, [&]() {
        failed = true;
        failedOnOwnerThread = QThread::currentThread() == ownerThread;
    });
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200), QByteArray("https://example.com"));

    VERIFY(window == &app.window);
    std::thread failureThread([&]() {
        app.NotifyChromeWindowInitializationFailed();
    });
    failureThread.join();
    VERIFY(!failed);
    // Teardown may overtake the owner-thread failure event.
    surface->backendDestroyed();
    processEvents();
    VERIFY(failed);
    VERIFY(failedOnOwnerThread);
    surface.clear();
}

void testLegacyWindowSelection()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200));

    VERIFY(window == &app.window);
    VERIFY(app.createCount == 1);
    VERIFY(app.chromeCreateCount == 0);
    VERIFY(eventIndex(app.events, "created") >= 0);
    VERIFY(eventIndex(app.events, "listener-set") >= 0);

    surface->requestDestroy();
    VERIFY(app.destroyCount == 1);
    VERIFY(eventIndex(app.events, "listener-cleared") <
           eventIndex(app.events, "destroyed"));
    surface->backendDestroyed();
    surface.clear();
}

void testWindowListenerForwarding()
{
    EmbedLiteApp app;
    RecordingWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200));

    VERIFY(window == &app.window);
    VERIFY(app.windowListener != &listener);
    app.windowListener->WindowInitialized();
    app.windowListener->CompositorCreated();
    app.windowListener->CompositingFinished();
    app.windowListener->DrawOverlay(nsIntRect());
    VERIFY(!app.windowListener->PreRender());
    VERIFY(listener.initialized == 1);
    VERIFY(listener.compositorCreated == 1);
    VERIFY(listener.compositingFinished == 1);
    VERIFY(listener.overlays == 1);

    listener.surfaceToClear = &surface;
    app.windowListener->WindowDestroyed();
    VERIFY(listener.destroyed == 1);
    VERIFY(surface.isNull());
}

void testChromeWindowFrameGate()
{
    VERIFY(QtMoz::windowFrameIsValid(
            true, true, false, false, true, true, true));
    VERIFY(!QtMoz::windowFrameIsValid(
            false, true, false, false, true, true, true));
    VERIFY(!QtMoz::windowFrameIsValid(
            true, false, false, false, true, true, true));
    VERIFY(!QtMoz::windowFrameIsValid(
            true, true, false, false, false, true, true));
    VERIFY(!QtMoz::windowFrameIsValid(
            true, true, false, false, true, false, true));
    VERIFY(!QtMoz::windowFrameIsValid(
            true, true, false, false, true, true, false));
    VERIFY(QtMoz::windowFrameIsValid(
            false, true, true, true, true, true, true));

    VERIFY(!QtMoz::chromeQuickWindowShouldDelete(
            true, false, false));
    VERIFY(!QtMoz::chromeQuickWindowShouldDelete(
            true, true, true));
    VERIFY(!QtMoz::chromeQuickWindowShouldDelete(
            false, true, false));
    VERIFY(QtMoz::chromeQuickWindowShouldDelete(
            true, true, false));
}

void testDestroyWaitsForFrameReleaseAndStop()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200));
    QMozSurfaceFrameToken readyToken = { 0, 0 };

    VERIFY(window == &app.window);
    VERIFY(surface->setPlatformFrameCallbacks(
            [&](const QMozSurfaceFrameToken &token) {
        readyToken = token;
    }, [&]() {
        app.events.push_back("qt-stopped");
    }));
    VERIFY(surface->setPlatformFrameDeliveryEnabled(true));
    window->NotifyFrameReady({ 7, 11 });
    processEvents();
    VERIFY(readyToken.epoch == 7);
    VERIFY(readyToken.sequence == 11);
    VERIFY(surface->acquirePlatformFrame(
            readyToken, [](const QMozSurfaceFrame &) {
        return true;
    }));

    surface->requestDestroy();
    VERIFY(eventIndex(app.events, "disable-blocked") >= 0);
    VERIFY(app.destroyCount == 0);

    VERIFY(surface->releasePlatformFrame({
        readyToken, QMozSurfaceFrameFenceType::NoHandle, nullptr
    }));
    processEvents();
    VERIFY(eventIndex(app.events, "disabled") >= 0);
    VERIFY(app.destroyCount == 0);

    window->NotifyFrameDeliveryStopped();
    processEvents();
    VERIFY(app.destroyCount == 1);
    VERIFY(eventIndex(app.events, "released") <
           eventIndex(app.events, "disabled"));
    VERIFY(eventIndex(app.events, "disabled") <
           eventIndex(app.events, "qt-stopped"));
    VERIFY(eventIndex(app.events, "qt-stopped") <
           eventIndex(app.events, "listener-cleared"));
    VERIFY(eventIndex(app.events, "listener-cleared") <
           eventIndex(app.events, "destroyed"));

    surface->backendDestroyed();
    VERIFY(!surface->releasePlatformFrame({
        readyToken, QMozSurfaceFrameFenceType::NoHandle, nullptr
    }));
    surface.clear();
}

void testFailedDisablePreservesQueuedFrame()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200));
    QMozSurfaceFrameToken readyToken = { 0, 0 };

    VERIFY(surface->setPlatformFrameCallbacks(
            [&](const QMozSurfaceFrameToken &token) {
        readyToken = token;
    }, QMozSurfaceFrameDeliveryStoppedCallback()));
    VERIFY(surface->setPlatformFrameDeliveryEnabled(true));
    window->NotifyFrameReady({ 13, 17 });
    processEvents();
    const QMozSurfaceFrameToken acquiredToken = readyToken;
    VERIFY(surface->acquirePlatformFrame(
            acquiredToken, [](const QMozSurfaceFrame &) {
        return true;
    }));

    std::thread readyThread([&]() {
        window->NotifyFrameReady({ 13, 19 });
    });
    readyThread.join();
    VERIFY(!surface->setPlatformFrameDeliveryEnabled(false));
    processEvents();
    VERIFY(readyToken.epoch == 13);
    VERIFY(readyToken.sequence == 19);

    VERIFY(surface->releasePlatformFrame({
        acquiredToken, QMozSurfaceFrameFenceType::NoHandle, nullptr
    }));
    VERIFY(surface->setPlatformFrameDeliveryEnabled(false));
    window->NotifyFrameDeliveryStopped();
    processEvents();
    surface->requestDestroy();
    VERIFY(app.destroyCount == 1);
    surface->backendDestroyed();
    surface.clear();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testRegistryLifetime();
    testReentrantRemoval();
    testPlatformFrameForwarding();
    testFrameStreamTracksLatestConsumerFrame();
    testQueuedFrameSuppressedAfterStop();
    testChromeWindowSelection();
    testChromeWindowFailureMarshalledToOwnerThread();
    testLegacyWindowSelection();
    testWindowListenerForwarding();
    testChromeWindowFrameGate();
    testDestroyWaitsForFrameReleaseAndStop();
    testFailedDisablePreservesQueuedFrame();
    return failures == 0 ? 0 : 1;
}
