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

#include <mozilla/embedlite/EmbedLiteView.h>
#include <Units.h>

#include "qmozwindow.h"
#include "qmozscrolldecorator.h"
#include "qmozview_templated_wrapper.h"
#include "qmozview_defined_wrapper.h"
#include "qmozsecurity.h"

class QTouchEvent;
class QMozContext;
class QMozWindow;

class QMozViewPrivate : public QObject,
    public mozilla::embedlite::EmbedLiteViewListener
{
    Q_OBJECT
public:
    enum DirtyStateBit {
        DirtySize = 0x0001,
        DirtyMargin = 0x0002
    };

    Q_DECLARE_FLAGS(DirtyState, DirtyStateBit)

    QMozViewPrivate(IMozQViewIface *aViewIface, QObject *publicPtr);
    virtual ~QMozViewPrivate();

    // EmbedLiteViewListener implementation:
    void ViewInitialized() override;
    void ViewDestroyed() override;
    void SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void OnLocationChanged(const char *aLocation, bool aCanGoBack, bool aCanGoForward) override;
    void OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal) override;
    void OnLoadStarted(const char *aLocation) override;
    void OnLoadFinished(void) override;
    void OnWindowCloseRequested() override;
    void RecvAsyncMessage(const char16_t *aMessage, const char16_t *aData) override;
    char *RecvSyncMessage(const char16_t *aMessage, const char16_t *aData) override;
    void OnLoadRedirect(void) override;
    void OnSecurityChanged(const char *aStatus, unsigned int aState) override;
    void OnFirstPaint(int32_t aX, int32_t aY) override;
    void OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight) override;
    void GetIMEStatus(int32_t *aIMEEnabled, int32_t *aIMEOpen) override;
    void IMENotification(int aIstate, bool aOpen, int aCause, int aFocusChange,
                         const char16_t *inputType, const char16_t *inputMode) override;
    void OnTitleChanged(const char16_t *aTitle) override;
    bool HandleLongTap(const nsIntPoint &aPoint) override;
    bool HandleSingleTap(const nsIntPoint &aPoint) override;
    bool HandleDoubleTap(const nsIntPoint &aPoint) override;
    bool HandleScrollEvent(bool aIsRootScrollFrame, const gfxRect &aContentRect, const gfxSize &aScrollableSize) override;

    void SetMargins(const QMargins &margins, bool updateTopBottom);
    QColor GetBackgroundColor() const;
    void SetIsFocused(bool aIsFocused);
    void SetThrottlePainting(bool aThrottle);
    void UpdateScrollArea(unsigned int aWidth, unsigned int aHeight, float aPosX, float aPosY);
    void TestFlickingMode(QTouchEvent *event);
    void HandleTouchEnd(bool &draggingChanged, bool &pinchingChanged);
    void ResetState();
    void UpdateMoving(bool moving);
    void ResetPainted();
    void ReceiveInputEvent(const mozilla::InputData &event);

    void scrollTo(int x, int y);
    void scrollBy(int x, int y);

    void setSize(const QSizeF &size);

    void load(const QString &url);
    void loadFrameScript(const QString &frameScript);
    void addMessageListener(const QString &name);
    void addMessageListeners(const QStringList &messageNamesList);

    void startMoveMonitor();
    void timerEvent(QTimerEvent *event);
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;
    void inputMethodEvent(QInputMethodEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void touchEvent(QTouchEvent *event);

    void sendAsyncMessage(const QString &name, const QVariant &value);
    void setMozWindow(QMozWindow *);

    mozilla::ScreenIntPoint createScreenPoint(const QPointF &point) const;
    mozilla::ScreenIntPoint createScreenPoint(const int &posX, const int &posY) const;

    QPointF renderingOffset() const;

public Q_SLOTS:
    void onCompositorCreated();

protected:
    friend class QOpenGLWebPage;
    friend class QuickMozView;

    void synthTouchBegin(const QVariant &touches);
    void synthTouchMove(const QVariant &touches);
    void synthTouchEnd(const QVariant &touches);
    void recvMouseMove(int posX, int posY);
    void recvMousePress(int posX, int posY);
    void recvMouseRelease(int posX, int posY);

    IMozQViewIface *mViewIface;
    QPointer<QObject> q;
    QPointer<QMozWindow> mMozWindow;
    QMozContext *mContext;
    mozilla::embedlite::EmbedLiteView *mView;
    bool mViewInitialized;
    QColor mBgColor;
    qreal mTopMargin;
    qreal mBottomMargin;
    QMargins mMargins;
    mutable QMutex mBgColorMutex;
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
    QString mLocation;
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
    Qt::InputMethodHints mInputMethodHints;
    bool mIsInputFieldFocused;
    bool mPreedit;
    bool mViewIsFocused;
    bool mHasContext;
    bool mPressed;
    bool mDragging;
    bool mFlicking;
    // Moving monitoring
    int mMovingTimerId;
    qreal mOffsetX;
    qreal mOffsetY;
    bool mHasCompositor;
    QMozSecurity mSecurity;

    DirtyState mDirtyState;

    QString mPendingUrl;
    QStringList mPendingMessageListeners;
    QStringList mPendingFrameScripts;
};

qint64 current_timestamp(QTouchEvent *);

#endif /* qmozview_p_h */
