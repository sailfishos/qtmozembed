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
#include "runtime/qmozchromesession_p.h"
#include "runtime/qmozchromewindowshutdown_p.h"
#include "backends/embedlite/embedlitechromesession_p.h"
#include "backends/embedlite/embedlitesurface_p.h"

#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteWindow.h"

#include <QCoreApplication>
#include <QString>
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

    QSharedPointer<QtMoz::QMozFrameStream> stream =
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
            surface, QSize(100, 200), false);
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
            surface, QSize(100, 200), true, initialUrl);

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

void testChromeTabWindowSelection()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const window = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200), true);

    VERIFY(window == &app.window);
    VERIFY(app.createCount == 0);
    VERIFY(app.chromeCreateCount == 0);
    VERIFY(app.chromeTabCreateCount == 1);
    VERIFY(eventIndex(app.events, "chrome-tab-created") >= 0);
    VERIFY(eventIndex(app.events, "listener-set") >= 0);

    surface->requestDestroy();
    VERIFY(app.destroyCount == 1);
    surface->backendDestroyed();
    surface.clear();
}

void testChromeSessionAdapter()
{
    EmbedLiteApp app;
    EmbedLiteWindowListener listener;
    QSharedPointer<QMozSurface> surface =
            QtMoz::createEmbedLiteSurface(&app, &listener);
    EmbedLiteWindow * const nativeWindow = QtMoz::reserveEmbedLiteSurface(
            surface, QSize(100, 200), true,
            QByteArray("https://example.com/chrome-smoke"));

    VERIFY(nativeWindow == &app.window);
    void *windowStorage = nullptr;
    QMozWindow * const window = fakeWindow(&windowStorage);
    int consumer = 0;
    VERIFY(QtMoz::chromeSessionUniqueId(&consumer) == 0);
    VERIFY(QtMoz::installWindowSurface(window, surface));

    std::string location;
    std::u16string title;
    int progress = 0;
    bool started = false;
    bool finished = false;
    bool destroyed = false;
    quint64 tabsRevision = 0;
    quint64 selectedTabId = 0;
    QVector<QMozChromeTabSnapshot> tabs;
    int tabSnapshotCount = 0;
    QMozChromeBeforeUnloadPrompt beforeUnloadPrompt;
    int beforeUnloadPromptCount = 0;
    QMozChromeInputContext inputContext;
    int inputContextCount = 0;
    QMozChromeSessionCallbacks callbacks;
    callbacks.locationChanged = [&](const char *value, bool canGoBack,
                                    bool canGoForward) {
        location = value;
        VERIFY(canGoBack);
        VERIFY(!canGoForward);
    };
    callbacks.loadStarted = [&](const char *) { started = true; };
    callbacks.loadFinished = [&]() { finished = true; };
    callbacks.loadProgress = [&](int value, qint64 current, qint64 total) {
        progress = value;
        VERIFY(current == 4);
        VERIFY(total == 10);
    };
    callbacks.titleChanged = [&](const char16_t *value) { title = value; };
    callbacks.tabsChanged = [&](
            quint64 revision, quint64 selected,
            const QVector<QMozChromeTabSnapshot> &snapshot) {
        ++tabSnapshotCount;
        tabsRevision = revision;
        selectedTabId = selected;
        tabs = snapshot;
    };
    callbacks.beforeUnloadPrompt = [&](
            const QMozChromeBeforeUnloadPrompt &prompt) {
        ++beforeUnloadPromptCount;
        beforeUnloadPrompt = prompt;
    };
    callbacks.inputContextChanged = [&inputContext, &inputContextCount](
            const QMozChromeInputContext &context) {
        ++inputContextCount;
        inputContext = context;
    };
    callbacks.destroyed = [&]() { destroyed = true; };
    VERIFY(QtMoz::attachChromeSession(&consumer, window, callbacks));
    VERIFY(!QtMoz::attachChromeSession(&consumer, window, callbacks));
    VERIFY(QtMoz::chromeSessionUniqueId(&consumer)
           == app.window.GetUniqueID());

    app.window.ChromeSession().NotifyState();
    VERIFY(location == "https://example.com/state");
    VERIFY(title == u"Example title");
    VERIFY(progress == 42);
    VERIFY(started);
    VERIFY(finished);

    app.window.ChromeTabSession().NotifyTabs();
    VERIFY(tabsRevision == 3);
    VERIFY(selectedTabId == 42);
    VERIFY(tabs.count() == 1);
    VERIFY(tabs.at(0).id == 42);
    VERIFY(tabs.at(0).persistentId == 77);
    VERIFY(tabs.at(0).location == QStringLiteral(
            "https://example.com/tab"));
    VERIFY(tabs.at(0).title == QStringLiteral("Tab title"));
    VERIFY(tabs.at(0).loading);
    VERIFY(!tabs.at(0).closing);
    VERIFY(!tabs.at(0).discarded);
    VERIFY(tabSnapshotCount == 1);
    app.window.ChromeTabSession().NotifyInvalidTabs();
    VERIFY(tabSnapshotCount == 1);
    VERIFY(tabsRevision == 3);

    app.window.ChromeTabSession().NotifyBeforeUnloadPrompt();
    VERIFY(beforeUnloadPromptCount == 1);
    VERIFY(beforeUnloadPrompt.requestId == 12345678901234567ULL);
    VERIFY(beforeUnloadPrompt.tabId == 42);
    VERIFY(beforeUnloadPrompt.persistentId == 77);
    VERIFY(beforeUnloadPrompt.title == QStringLiteral("Leave this page?"));
    VERIFY(beforeUnloadPrompt.text == QStringLiteral(
            "Changes you made may not be saved."));
    VERIFY(beforeUnloadPrompt.leaveLabel == QStringLiteral("Leave"));
    VERIFY(beforeUnloadPrompt.stayLabel == QStringLiteral("Stay"));

    std::u16string inputType = u"email";
    std::u16string inputMode = u"latin";
    std::u16string actionHint = u"send";
    app.window.ChromeInputSession().NotifyInputContext(
            inputType.c_str(), inputMode.c_str(), actionHint.c_str());
    inputType[0] = u'x';
    inputMode[0] = u'x';
    actionHint[0] = u'x';
    VERIFY(inputContextCount == 1);
    VERIFY(inputContext.enabled == 2);
    VERIFY(inputContext.open == 1);
    VERIFY(inputContext.inputType == QStringLiteral("email"));
    VERIFY(inputContext.inputMode == QStringLiteral("latin"));
    VERIFY(inputContext.actionHint == QStringLiteral("send"));
    VERIFY(inputContext.cause == 3);
    VERIFY(inputContext.focusChange == 4);

    VERIFY(QtMoz::chromeSessionSendTextEvent(
            &consumer, QString::fromUtf8("committed \xc3\xa4"),
            QString::fromUtf8("preedit \xe2\x82\xac"), -2, 5));
    VERIFY(app.window.ChromeInputSession().lastCommit ==
           "committed \xc3\xa4");
    VERIFY(app.window.ChromeInputSession().lastPreedit ==
           "preedit \xe2\x82\xac");
    VERIFY(app.window.ChromeInputSession().lastReplacementStart == -2);
    VERIFY(app.window.ChromeInputSession().lastReplacementLength == 5);
    VERIFY(QtMoz::chromeSessionSendKeyPress(&consumer, 13, 2, 65));
    VERIFY(app.window.ChromeInputSession().lastPressDomKeyCode == 13);
    VERIFY(app.window.ChromeInputSession().lastPressModifiers == 2);
    VERIFY(app.window.ChromeInputSession().lastPressCharCode == 65);
    VERIFY(QtMoz::chromeSessionSendKeyRelease(&consumer, 27, 4, 66));
    VERIFY(app.window.ChromeInputSession().lastReleaseDomKeyCode == 27);
    VERIFY(app.window.ChromeInputSession().lastReleaseModifiers == 4);
    VERIFY(app.window.ChromeInputSession().lastReleaseCharCode == 66);

    QMozChromeHistoryEntry historyEntry;
    historyEntry.location = QStringLiteral("https://example.com/restored");
    historyEntry.title = QStringLiteral("Restored title");
    QMozChromeRestoredTab restoredTab;
    restoredTab.persistentId = 91;
    restoredTab.history.append(historyEntry);
    restoredTab.selectedHistoryIndex = 0;
    QVector<QMozChromeRestoredTab> restoredTabs;
    restoredTabs.append(restoredTab);
    VERIFY(QtMoz::chromeSessionRestoreTabs(&consumer, restoredTabs, 0));
    VERIFY(app.window.ChromeTabSession().lastRestoreCount == 1);
    VERIFY(app.window.ChromeTabSession().lastRestoreSelectedIndex == 0);
    VERIFY(app.window.ChromeTabSession().lastRestorePersistentId == 91);
    VERIFY(app.window.ChromeTabSession().lastRestoreLocation ==
           "https://example.com/restored");
    VERIFY(QtMoz::chromeSessionNewTab(
            &consumer, QStringLiteral("https://example.com/new"),
            92, true, true));
    VERIFY(app.window.ChromeTabSession().lastURL ==
           "https://example.com/new");
    VERIFY(app.window.ChromeTabSession().lastPersistentId == 92);
    VERIFY(app.window.ChromeTabSession().lastFromExternal);
    VERIFY(app.window.ChromeTabSession().lastInBackground);
    VERIFY(QtMoz::chromeSessionAssociateTab(&consumer, 42, 93));
    VERIFY(app.window.ChromeTabSession().lastTabId == 42);
    VERIFY(app.window.ChromeTabSession().lastPersistentId == 93);
    VERIFY(QtMoz::chromeSessionSelectTab(&consumer, 42));
    VERIFY(QtMoz::chromeSessionCloseTab(&consumer, 42));
    VERIFY(QtMoz::chromeSessionResolveBeforeUnloadPrompt(
            &consumer, 12345678901234567ULL, 42, true));
    VERIFY(app.window.ChromeTabSession().lastBeforeUnloadRequestId
           == 12345678901234567ULL);
    VERIFY(app.window.ChromeTabSession().lastBeforeUnloadTabId == 42);
    VERIFY(app.window.ChromeTabSession().lastBeforeUnloadPermit);

    VERIFY(QtMoz::chromeSessionLoadURL(
            &consumer, QStringLiteral("https://example.com/next"), true));
    VERIFY(app.window.ChromeSession().lastURL ==
           "https://example.com/next");
    VERIFY(app.window.ChromeSession().lastFromExternal);
    VERIFY(QtMoz::chromeSessionGoBack(&consumer));
    VERIFY(QtMoz::chromeSessionGoForward(&consumer));
    VERIFY(QtMoz::chromeSessionStop(&consumer));
    VERIFY(QtMoz::chromeSessionReload(&consumer, true));
    VERIFY(app.window.ChromeSession().lastHardReload);
    VERIFY(QtMoz::chromeSessionSetActive(&consumer, true));
    VERIFY(app.window.ChromeSession().lastActive);
    VERIFY(QtMoz::chromeSessionSetFocused(&consumer, true));
    VERIFY(app.window.ChromeSession().lastFocused);

    mozilla::embedlite::EmbedTouchInput touch(
            mozilla::embedlite::EmbedTouchInput::MULTITOUCH_START, 123);
    touch.touches.push_back(mozilla::embedlite::TouchData(
            7, mozilla::embedlite::TouchPointF(20.0f, 30.0f), 0.5f));
    VERIFY(QtMoz::chromeSessionReceiveInputEvent(&consumer, touch));
    VERIFY(app.window.ChromeSession().lastTouchType
           == mozilla::embedlite::EmbedTouchInput::MULTITOUCH_START);
    VERIFY(app.window.ChromeSession().lastTouchTimeStamp == 123);
    VERIFY(app.window.ChromeSession().lastTouchCount == 1);
    VERIFY(app.window.ChromeSession().lastTouchIdentifier == 7);
    VERIFY(app.window.ChromeSession().lastTouchX == 20.0f);
    VERIFY(app.window.ChromeSession().lastTouchY == 30.0f);
    VERIFY(app.window.ChromeSession().lastTouchPressure == 0.5f);

    app.window.ChromeSession().NotifyDestroyed();
    VERIFY(!destroyed);
    app.window.ChromeTabSession().NotifyDestroyed();
    VERIFY(!destroyed);
    app.window.ChromeInputSession().NotifyDestroyed();
    VERIFY(destroyed);
    VERIFY(QtMoz::chromeSessionUniqueId(&consumer) == 0);
    VERIFY(!QtMoz::chromeSessionGoBack(&consumer));
    VERIFY(!QtMoz::chromeSessionReceiveInputEvent(&consumer, touch));
    VERIFY(!QtMoz::chromeSessionSendTextEvent(
            &consumer, QStringLiteral("text"), QString(), 0, 0));
    VERIFY(QtMoz::attachChromeSession(&consumer, window, callbacks));
    VERIFY(QtMoz::chromeSessionUniqueId(&consumer)
           == app.window.GetUniqueID());
    QtMoz::detachChromeSession(&consumer);
    VERIFY(QtMoz::chromeSessionUniqueId(&consumer) == 0);
    VERIFY(!QtMoz::chromeSessionSetActive(&consumer, false));
    app.window.SetChromeHosted(false);
    VERIFY(!QtMoz::attachChromeSession(&consumer, window, callbacks));

    VERIFY(QtMoz::takeWindowSurface(window) == surface);
    surface->requestDestroy();
    surface->backendDestroyed();
    surface.clear();
}

void testChromeWindowShutdownBarrier()
{
    QtMoz::QMozChromeWindowShutdown shutdown;
    int windowA = 0;
    int windowB = 0;
    int drainsA = 0;
    int drainsB = 0;
    int releasesA = 0;
    int releasesB = 0;
    QtMoz::QMozChromeWindowDrainComplete completeA;
    QtMoz::QMozChromeWindowDrainComplete completeB;

    VERIFY(shutdown.track(
            &windowA,
            [&](const QtMoz::QMozChromeWindowDrainComplete &complete) {
        ++drainsA;
        completeA = complete;
    }, [&]() {
        ++releasesA;
    }));
    VERIFY(shutdown.track(
            &windowB,
            [&](const QtMoz::QMozChromeWindowDrainComplete &complete) {
        ++drainsB;
        completeB = complete;
    }, [&]() {
        ++releasesB;
    }));
    VERIFY(!shutdown.track(
            &windowA,
            [](const QtMoz::QMozChromeWindowDrainComplete &) {},
            []() {}));

    VERIFY(shutdown.beginStop());
    VERIFY(shutdown.isStopping());
    VERIFY(shutdown.isWaiting());
    VERIFY(drainsA == 1);
    VERIFY(drainsB == 1);
    VERIFY(releasesA == 0);
    VERIFY(releasesB == 0);

    completeA();
    completeA();
    VERIFY(releasesA == 1);
    VERIFY(releasesB == 0);
    VERIFY(shutdown.windowReleased(&windowA));
    VERIFY(!shutdown.windowReleased(&windowA));
    VERIFY(shutdown.isWaiting());

    // A missing completion deliberately keeps the second native window live.
    VERIFY(shutdown.beginStop());
    VERIFY(drainsB == 1);
    VERIFY(releasesB == 0);
    completeB();
    VERIFY(releasesB == 1);
    VERIFY(shutdown.windowReleased(&windowB));
    VERIFY(!shutdown.isWaiting());
    VERIFY(!shutdown.beginStop());
}

void testChromeWindowShutdownSynchronousRelease()
{
    QtMoz::QMozChromeWindowShutdown shutdown;
    int windowA = 0;
    int windowB = 0;
    int releases = 0;
    VERIFY(shutdown.track(
            &windowA,
            [](const QtMoz::QMozChromeWindowDrainComplete &complete) {
        complete();
    }, [&]() {
        ++releases;
        VERIFY(shutdown.windowReleased(&windowA));
    }));
    VERIFY(shutdown.track(
            &windowB,
            [](const QtMoz::QMozChromeWindowDrainComplete &complete) {
        complete();
    }, [&]() {
        ++releases;
        VERIFY(shutdown.windowReleased(&windowB));
    }));

    VERIFY(!shutdown.beginStop());
    VERIFY(releases == 2);
    VERIFY(!shutdown.isWaiting());
}

void testChromeWindowDrainCompletionRetainsState()
{
    int window = 0;
    int releases = 0;
    QtMoz::QMozChromeWindowDrainComplete complete;
    {
        QtMoz::QMozChromeWindowShutdown shutdown;
        VERIFY(shutdown.track(
                &window,
                [&](const QtMoz::QMozChromeWindowDrainComplete &callback) {
            complete = callback;
        }, [&]() {
            ++releases;
        }));
        VERIFY(shutdown.beginDrain(&window));
    }

    complete();
    complete();
    VERIFY(releases == 1);
}

void testChromeWindowDrainPrecedesNativeDestroy()
{
    void *storage = nullptr;
    QMozWindow * const window = fakeWindow(&storage);
    int destructions = 0;
    int consumer = 0;
    QSharedPointer<FakeSurface> surface(
            new FakeSurface(window, &destructions));
    VERIFY(QtMoz::installWindowSurface(window, surface));
    VERIFY(QtMoz::installWindowFrameStream(window, surface));
    QtMoz::setWindowFrameConsumer(window, &consumer, []() {});
    VERIFY(QtMoz::startWindowFrameStream(window));

    const QMozSurfaceFrameToken token = { 29, 31 };
    surface->notifyFrameReady(token);
    QSharedPointer<QtMoz::QMozFrameStream> stream =
            QtMoz::windowFrameStream(window);
    QMozSurfaceFrameToken pending = { 0, 0 };
    VERIFY(stream->takePendingFrame(&consumer, &pending));
    VERIFY(surface->acquirePlatformFrame(
            pending, [](const QMozSurfaceFrame &) {
        return true;
    }));

    QtMoz::QMozChromeWindowShutdown shutdown;
    QtMoz::QMozChromeWindowDrainComplete drainComplete;
    VERIFY(shutdown.track(
            window,
            [&](const QtMoz::QMozChromeWindowDrainComplete &complete) {
        QtMoz::clearWindowFrameConsumer(window, &consumer);
        drainComplete = complete;
    }, [&]() {
        surface->requestDestroy();
    }));
    VERIFY(shutdown.beginStop());
    VERIFY(surface->destroyRequests() == 0);

    VERIFY(surface->releasePlatformFrame({
        token, QMozSurfaceFrameFenceType::NoHandle, nullptr
    }));
    drainComplete();
    VERIFY(surface->destroyRequests() == 1);

    QtMoz::takeWindowFrameStream(window)->backendDestroyed();
    stream.clear();
    surface.clear();
    VERIFY(destructions == 1);
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
            surface, QSize(100, 200), true,
            QByteArray("https://example.com"));

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
            surface, QSize(100, 200), false);

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
            surface, QSize(100, 200), false);

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
            surface, QSize(100, 200), false);
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
            surface, QSize(100, 200), false);
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
    testChromeTabWindowSelection();
    testChromeSessionAdapter();
    testChromeWindowShutdownBarrier();
    testChromeWindowShutdownSynchronousRelease();
    testChromeWindowDrainCompletionRetainsState();
    testChromeWindowDrainPrecedesNativeDestroy();
    testChromeWindowFailureMarshalledToOwnerThread();
    testLegacyWindowSelection();
    testWindowListenerForwarding();
    testChromeWindowFrameGate();
    testDestroyWaitsForFrameReleaseAndStop();
    testFailedDisablePreservesQueuedFrame();
    return failures == 0 ? 0 : 1;
}
