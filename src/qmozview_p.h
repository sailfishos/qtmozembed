/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qmozview_p_h
#define qmozview_p_h

#include <QColor>
#include <QImage>
#include <QSize>
#include <QTime>
#include <QString>
#include <QPointer>
#include <QPointF>
#include <QMutex>
#include <QMap>
#include <QSGSimpleTextureNode>
#include <QKeyEvent>
#include <QJSValue>
#include <QVariantList>
#include <QVector>

#include <mozilla/embedlite/EmbedInputData.h>
#include "qmozwindow.h"
#include "qmozscrolldecorator.h"
#include "qmozview_templated_wrapper.h"
#include "qmozview_defined_wrapper.h"
#include "qmozsecurity.h"
#include "runtime/qmozchromesession_p.h"

class QTouchEvent;
class QAbstractItemModel;
class QMozContext;
class QMozTabModel;
class QMozWindow;

namespace mozilla {
namespace embedlite {
class EmbedTouchInput;
class TouchPointF;
}
}

class QMozViewPrivate : public QObject
{
    Q_OBJECT
public:
    enum DirtyStateBit {
        DirtySize = 0x0001,
        DirtyMargin = 0x0002,
        DirtyDynamicToolbarHeight = 0x0004,
        DirtyScreenProperties = 0x0008,
        DirtySafeAreaInsets = 0x0020,
        DirtyDesktopMode = 0x0040,
        DirtyThrottlePainting = 0x0080,
        DirtyHttpUserAgent = 0x0100,
    };

    Q_DECLARE_FLAGS(DirtyState, DirtyStateBit)

    QMozViewPrivate(IMozQViewIface *aViewIface, QObject *publicPtr);
    virtual ~QMozViewPrivate();

    void ViewInitialized();
    void ViewDestroyed();
    void SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void OnLocationChanged(const char *aLocation, bool aCanGoBack, bool aCanGoForward);
    void OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal);
    void OnLoadStarted(const char *aLocation);
    void OnLoadFinished();
    void OnWindowCloseRequested();
    void OnLoadRedirect();
    void OnSecurityChanged(const char *aStatus, unsigned int aState);
    void OnFirstPaint(int32_t aX, int32_t aY);
    void OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight);
    void IMENotification(int aIstate, bool aOpen, int aCause, int aFocusChange,
                         const char16_t *inputType, const char16_t *inputMode);
    void OnTitleChanged(const char16_t *aTitle);

    // Starting from here these are QMozViewPrivate methods.
    void setDynamicToolbarHeight(const int height);
    void setMargins(const QMargins &margins, bool updateTopBottom);
    void setSafeAreaInsets(const QMargins &insets);
    void setIsFocused(bool aIsFocused);
    void setDesktopMode(bool aDesktopMode);
    void setJavascriptEnabled(bool aEnabled);
    void setThrottlePainting(bool aThrottle);
    void updateScrollArea(unsigned int aWidth, unsigned int aHeight, float aPosX, float aPosY);
    void testFlickingMode(QTouchEvent *event);
    void handleTouchEnd(bool &draggingChanged, bool &pinchingChanged);
    void resetTouchState();
    void updateMoving(bool moving);
    void reset();
    void receiveInputEvent(const mozilla::embedlite::EmbedTouchInput &event);
    void setHttpUserAgent(const QString &httpUserAgent);
    QString httpUserAgent() const;

    void scrollTo(int x, int y);
    void scrollBy(int x, int y);

    void runJavaScript(const QString &script,
                       const QJSValue &callback,
                       const QJSValue &errorCallback);
    bool domContentLoaded() const;

    void setSize(const QSizeF &size);
    void setScreenProperties(int depth, qreal dpi);

    QUrl url() const;
    bool isUrlResolved() const;
    void goBack();
    void goForward();
    void stop();
    void cancelPendingNavigation();
    void reload();
    void load(const QString &url, bool fromExternal);
    void clearPendingUrl();
    void loadFrameScript(const QString &frameScript);
    void addMessageListener(const std::string &name);
    void addMessageListeners(const std::vector<std::string> &messageNamesList);

    void startMoveMonitor();
    void timerEvent(QTimerEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;
    void inputMethodEvent(QInputMethodEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void touchEvent(QTouchEvent *event);
    void wheelEvent(QWheelEvent *event);

    void sendAsyncMessage(const QString &message, const QVariant &value);
    bool sendAsyncMessageToTab(
            const QString &tabId, const QString &message,
            const QVariant &value);
    void setMozWindow(QMozWindow *);
    bool attachChromeSession();

    QAbstractItemModel *tabModel() const;
    QString selectedTabId() const;
    int selectedTabIndex() const;
    bool restoreTabs(const QVariantList &tabs, int selectedTabIndex);
    bool newTab(const QString &url, const QString &persistentId,
                bool fromExternal, bool inBackground);
    bool associateTab(const QString &tabId, const QString &persistentId);
    bool selectTab(const QString &tabId);
    bool closeTab(const QString &tabId);
    void updateChromeTabs(
            quint64 revision, quint64 selectedTabId,
            const QVector<QMozChromeTabSnapshot> &tabs);
    void updateChromeContentState(const QMozChromeContentState &state);
    void recvChromeAsyncMessage(
            quint64 tabId, quint64 persistentId, quint64 locationRevision,
            const QString &message, const QString &json);
    void chromeWindowCloseRequested(
            quint64 tabId, quint64 persistentId);
    void showChromeBeforeUnloadPrompt(
            const QMozChromeBeforeUnloadPrompt &prompt);
    void chromeTabCloseResult(quint64 tabId, bool closed);
    void clearChromeTabs();
    quint64 selectedChromeTabId() const;
    bool fullscreen() const;

    void setParentId(unsigned parentId);
    void setParentBrowsingContext(uintptr_t parentBrowsingContext);
    void setHidden(bool hidden);
    void setChromeGestureEnabled(bool value);
    void setChromeGestureThreshold(qreal value);
    void setChrome(bool value);

    mozilla::embedlite::TouchPointF createEmbedTouchPoint(const QPointF &point) const;
    mozilla::embedlite::TouchPointF createEmbedTouchPoint(qreal posX, qreal posY) const;

    QPointF renderingOffset() const;

    void applyAutoCorrect();

public Q_SLOTS:
    void onCompositorCreated();
    void updateLoaded();
    void createView();

protected:
    friend class QuickMozView;
    friend class QMozNativeView;

    void synthTouchBegin(const QVariant &touches);
    void synthTouchMove(const QVariant &touches);
    void synthTouchEnd(const QVariant &touches);
    void recvMouseMove(int posX, int posY);
    void recvMousePress(int posX, int posY);
    void recvMouseRelease(int posX, int posY);
    void recvMouseEvent(
            QMouseEvent *event, QMozChromeMouseType type);

    void doSendAsyncMessage(const QString &message, const QVariant &value);
    bool doSendAsyncMessageToTab(
            quint64 tabId, const QString &message, const QVariant &value);
    bool handleAsyncMessage(const QString &message, const QVariant &data,
                            quint64 tabId = 0,
                            quint64 persistentId = 0);
    void queuePendingFrameScript(const QString &frameScript);
    void queuePendingMessageListener(const std::string &name);
    void removePendingMessageListener(const std::string &name);
    void flushPendingChromeRegistrations();
    void clearDirtyDynamicToolbarHeight();
    qreal screenDensity() const;
    void sendScreenProperties();
    void applyChromePageSettings();

    struct PendingInputMethodEvent {
        QString commit;
        QString preedit;
        int replacementStart;
        int replacementLength;
        qint64 replacementOffset;
        bool hadPreedit;
    };
    void dispatchInputMethodEvent(const PendingInputMethodEvent &event);
    void clearPendingInputMethodEvents();
    void flushPendingInputMethodEvents();

    IMozQViewIface *mViewIface;
    QPointer<QObject> q;
    QPointer<QMozWindow> mMozWindow;
    QMozContext *mContext;
    bool mViewInitialized;
    unsigned mParentID;
    uintptr_t mParentBrowsingContext;
    bool mPrivateMode;
    bool mHidden;
    bool mDesktopMode;
    bool mJavascriptEnabled;
    bool mThrottlePainting;
    bool mActive;
    bool mLoaded;
    bool mDOMContentLoaded;
    QColor mBackgroundColor;
    qreal mTopMargin;
    qreal mBottomMargin;
    int mDynamicToolbarHeight;
    QMargins mMargins;
    QMargins mSafeAreaInsets;
    QImage mTempBufferImage;
    QSGTexture *mTempTexture;
    bool mEnabled;
    bool mChromeGestureEnabled;
    qreal mChromeGestureThreshold;
    bool mChrome;
    qreal mMoveDelta;
    qreal mDragStartY;
    bool mMoving;
    bool mPinching;
    QSizeF mSize;
    qint64 mLastTimestamp;
    qint64 mLastStationaryTimestamp;
    QPointF mLastPos;
    QPointF mSecondLastPos;
    QPointF mLastStationaryPos;
    QMap<int, QPointF> mActiveTouchPoints;
    bool mCanFlick;
    bool mPendingTouchEvent;
    QString mUrl;
    QString mTitle;
    int mProgress;
    bool mCanGoBack;
    bool mCanGoForward;
    bool mIsLoading;
    bool mLastIsGoodRotation;
    bool mIsPasswordField;
    bool mGraphicsViewAssigned;
    QRectF mContentRect;
    QSizeF mScrollableSize;
    QPointF mScrollableOffset;
    bool mAtXBeginning;
    bool mAtXEnd;
    bool mAtYBeginning;
    bool mAtYEnd;
    // Non visual
    QMozScrollDecorator mVerticalScrollDecorator;
    QMozScrollDecorator mHorizontalScrollDecorator;
    float mContentResolution;
    bool mIsPainted;
    bool mFullscreen;
    Qt::InputMethodHints mInputMethodHints;
    Qt::InputMethodHints mInputMethodAttributes;
    QVariant mSurroundingText;
    QVariant mCursorPosition;
    QVariant mAnchorPosition;
    bool mIsInputFieldFocused;
    bool mPreedit;
    bool mWaitingForBackspaceInputContext;
    QVector<PendingInputMethodEvent> mPendingInputMethodEvents;
    bool mViewIsFocused;
    bool mPressed;
    bool mDragging;
    bool mFlicking;
    // Moving monitoring
    int mMovingTimerId;
    qreal mOffsetX;
    qreal mOffsetY;
    bool mHasCompositor;
    QMozSecurity mSecurity;
    int mDepth;
    qreal mDpi;
    struct PendingJSCall {
        QJSValue callback;
        QJSValue errorCallback;
        quint64 tabId;
        quint64 persistentId;
    };
    QMap<uint, PendingJSCall> mPendingJSCalls;
    uint mNextJSCallId;
    QString mHttpUserAgent;
    bool mAutoCompleteActive;
    QStringList mAutoCompleteList;

    QMozTabModel *mTabModel;
    quint64 mTabSnapshotRevision;
    bool mHasTabSnapshot;
    quint64 mContentStateTabId;
    quint64 mContentStatePersistentId;
    quint64 mContentStateLocationRevision;
    quint64 mContentStateRevision;
    QVector<QMozChromeRestoredTab> mPendingRestoredTabs;
    int mPendingSelectedTabIndex;
    bool mRestoreRequested;
    bool mRestorePending;

    DirtyState mDirtyState;

    QString mPendingUrl;
    bool mPendingFromExternal;
    quint64 mPendingUrlTabId;
    quint64 mPendingUrlLocationRevision;
    quint64 mPendingUrlSnapshotRevision;
    bool mPendingUrlSawLoading;
    std::vector<std::string> mPendingMessageListeners;
    QStringList mPendingFrameScripts;
};

#endif /* qmozview_p_h */
