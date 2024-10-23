/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "QMozViewPrivate"

#include <QGuiApplication>
#include <QJSValue>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTouchEvent>
#include <QQuickWindow>
#include <QScreen>
#include <QQmlInfo>

#include <iostream>
#include <locale>
#include <string>
#include <codecvt>

#include <mozilla/embedlite/EmbedInputData.h>
#include <mozilla/embedlite/EmbedLiteApp.h>
#include <mozilla/gfx/Tools.h>
#include <sys/time.h>
#include <mozilla/TimeStamp.h>

#include <nsPoint.h>

#include "qmozview_p.h"
#include "qmozwindow_p.h"
#include "qmozcontext.h"
#include "qmozenginesettings.h"
#include "EmbedQtKeyUtils.h"
#include "qmozembedlog.h"

#include "quickmozview.h"

#ifndef MOZVIEW_FLICK_THRESHOLD
#define MOZVIEW_FLICK_THRESHOLD 200
#endif

#ifndef MOZVIEW_FLICK_STOP_TIMEOUT
#define MOZVIEW_FLICK_STOP_TIMEOUT 500
#endif

#define SCROLL_EPSILON 0.001
#define SCROLL_BOUNDARY_EPSILON 0.05

using namespace mozilla;
using namespace mozilla::embedlite;

#define CONTENT_LOADED "chrome:contentloaded"
#define RUN_JAVASCRIPT "embedui:runjavascript"
#define RUN_JAVASCRIPT_REPLY "embed:runjavascript"
#define FORMASSIST_RESULT "FormAssist:AutoCompleteResult"
#define FORMASSIST_HIDE "FormAssist:Hide"
#define INPUTMETHOD_SET_INPUT_CONTEXT "InputMethodHandler:SetInputContext"
#define INPUTMETHOD_RESET_INPUT_CONTEXT "InputMethodHandler:ResetInputContext"
#define INPUTMETHOD_SET_INPUT_ATTRIBUTES "InputMethodHandler:SetInputAttributes"
#define INPUTMETHOD_RESET_INPUT_ATTRIBUTES "InputMethodHandler:ResetInputAttributes"
#define DOCURI_KEY "docuri"
#define ABOUT_URL_PREFIX "about:"

qint64 current_timestamp(QTouchEvent *aEvent)
{
    if (aEvent) {
        return aEvent->timestamp();
    }

    struct timeval te;
    gettimeofday(&te, nullptr);
    qint64 milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

// Map window size to orientation.
QSize contentWindowSize(const QMozWindow *window) {
    Q_ASSERT(window);

    Qt::ScreenOrientation orientation = window->pendingOrientation();
    QSize s = window->size();
    if (orientation == Qt::LandscapeOrientation || orientation == Qt::InvertedLandscapeOrientation) {
        s.transpose();
    }
    return s;
}

QMozViewPrivate::QMozViewPrivate(IMozQViewIface *aViewIface, QObject *publicPtr)
    : mViewIface(aViewIface)
    , q(publicPtr)
    , mMozWindow(nullptr)
    , mContext(nullptr)
    , mView(nullptr)
    , mViewInitialized(false)
    , mParentID(0)
    , mParentBrowsingContext(0)
    , mPrivateMode(false)
    , mDesktopMode(false)
    , mActive(false)
    , mLoaded(false)
    , mDOMContentLoaded(false)
    , mBackgroundColor(Qt::white)
    , mTopMargin(0.0)
    , mBottomMargin(0.0)
    , mDynamicToolbarHeight(0)
    , mMargins(0, 0, 0, 0)
    , mTempTexture(nullptr)
    , mEnabled(true)
    , mChromeGestureEnabled(true)
    , mChromeGestureThreshold(0.0)
    , mChrome(true)
    , mMoveDelta(0.0)
    , mDragStartY(0.0)
    , mMoving(false)
    , mPinching(false)
    , mSize(0.0, 0.0)
    , mLastTimestamp(0)
    , mLastStationaryTimestamp(0)
    , mLastPos(0.0, 0.0)
    , mSecondLastPos(0.0, 0.0)
    , mLastStationaryPos(0.0, 0.0)
    , mCanFlick(false)
    , mPendingTouchEvent(false)
    , mProgress(0)
    , mCanGoBack(false)
    , mCanGoForward(false)
    , mIsLoading(false)
    , mLastIsGoodRotation(true)
    , mIsPasswordField(false)
    , mGraphicsViewAssigned(false)
    , mContentRect(0.0, 0.0, 0.0, 0.0)
    , mScrollableSize(0.0, 0.0)
    , mScrollableOffset(0, 0)
    , mAtXBeginning(false)
    , mAtXEnd(false)
    , mAtYBeginning(false)
    , mAtYEnd(false)
    , mContentResolution(0.0)
    , mIsPainted(false)
    , mInputMethodHints(0)
    , mInputMethodAttributes(0)
    , mIsInputFieldFocused(false)
    , mPreedit(false)
    , mViewIsFocused(false)
    , mPressed(false)
    , mDragging(false)
    , mFlicking(false)
    , mMovingTimerId(0)
    , mOffsetX(0.0)
    , mOffsetY(0.0)
    , mHasCompositor(false)
    , mDepth(0)
    , mDpi(0.0)
    , mNextJSCallId(0)
    , mAutoCompleteActive(false)
    , mAutoCompleteList()
    , mDirtyState(0)
{
    loadFrameScript(QStringLiteral("chrome://embedlite/content/embedhelper.js"));
    addMessageListener(RUN_JAVASCRIPT_REPLY);
    addMessageListener(FORMASSIST_RESULT);
    addMessageListener(FORMASSIST_HIDE);
    addMessageListener(INPUTMETHOD_SET_INPUT_CONTEXT);
    addMessageListener(INPUTMETHOD_RESET_INPUT_CONTEXT);
    addMessageListener(INPUTMETHOD_SET_INPUT_ATTRIBUTES);
    addMessageListener(INPUTMETHOD_RESET_INPUT_ATTRIBUTES);
}

QMozViewPrivate::~QMozViewPrivate()
{
    delete mViewIface;
    mViewIface = nullptr;
    mViewInitialized = false;
    // Destroyed by EmbedLiteApp::ViewDestroyed but
    // cannot really be used after destroyed.
    mView = nullptr;
}

void QMozViewPrivate::updateScrollArea(unsigned int aWidth, unsigned int aHeight, float aPosX, float aPosY)
{
    bool widthChanged = false;
    bool heightChanged = false;
    // Emit changes only after both values have been updated.
    if (mScrollableSize.width() != aWidth) {
        mScrollableSize.setWidth(aWidth);
        widthChanged = true;
    }

    if (mScrollableSize.height() != aHeight) {
        mScrollableSize.setHeight(aHeight);
        heightChanged = true;
    }

    if (!gfx::FuzzyEqual(mScrollableOffset.x(), aPosX, SCROLL_EPSILON) ||
            !gfx::FuzzyEqual(mScrollableOffset.y(), aPosY, SCROLL_EPSILON)) {

        mScrollableOffset.setX(aPosX);
        mScrollableOffset.setY(aPosY);
        mViewIface->scrollableOffsetChanged();

        if (mEnabled) {
            // We could add moving timers for both of these and check them separately.
            // Currently we have only one timer event for content.
            mVerticalScrollDecorator.setMoving(true);
            mHorizontalScrollDecorator.setMoving(true);

            // Update vertical scroll decorator
            qreal ySizeRatio = mContentRect.height() * mContentResolution / mScrollableSize.height();
            qreal tmpValue = mMozWindow->size().height() * ySizeRatio;
            mVerticalScrollDecorator.setSize(tmpValue);
            tmpValue = mScrollableOffset.y() * ySizeRatio;
            mVerticalScrollDecorator.setPosition(tmpValue);

            // Update horizontal scroll decorator
            qreal xSizeRatio = mContentRect.width() * mContentResolution / mScrollableSize.width();
            tmpValue = mMozWindow->size().width() * xSizeRatio;
            mHorizontalScrollDecorator.setSize(tmpValue);
            tmpValue = mScrollableOffset.x() * xSizeRatio;
            mHorizontalScrollDecorator.setPosition(tmpValue);
        }

        // chrome, chromeGestureEnabled, and chromeGestureThreshold can be used
        // to control chrome/chromeless mode.
        // When chromeGestureEnabled is false, no actions are taken
        // When chromeGestureThreshold is true, chrome is set false when chromeGestrureThreshold is exceeded (pan/flick)
        // and set to true when flicking/panning the same amount to the the opposite direction.
        // This do not have relationship to HTML5 fullscreen API.
        if (mEnabled && mChromeGestureEnabled && mDragStartY >= 0.0) {
            // In MozView coordinates
            qreal offset = aPosY;
            qreal currentDelta = offset - mDragStartY;
            qCDebug(lcEmbedLiteExt) << "dragStartY:" << mDragStartY << "," << offset
                                    << "," << currentDelta << "," << mMoveDelta
                                    << "," << (qAbs(currentDelta) < mMoveDelta);

            if (qAbs(currentDelta) < mMoveDelta) {
                mDragStartY = offset;
            }

            if (currentDelta > mChromeGestureThreshold) {
                qCDebug(lcEmbedLiteExt) << "currentDelta > mChromeGestureThreshold:" << mChrome;
                if (mChrome) {
                    mChrome = false;
                    mViewIface->chromeChanged();
                }
            } else if (currentDelta < -mChromeGestureThreshold) {
                qCDebug(lcEmbedLiteExt) << "currentDelta < -mChromeGestureThreshold:" << mChrome;
                if (!mChrome) {
                    mChrome = true;
                    mViewIface->chromeChanged();
                }
            }
            mMoveDelta = qAbs(currentDelta);
        }
    }

    // determine if the viewport is panned to any edges
    bool oldAXB = mAtXBeginning;
    bool oldAXE = mAtXEnd;
    bool oldAYB = mAtYBeginning;
    bool oldAYE = mAtYEnd;
    mAtXBeginning = aPosX == 0 || gfx::FuzzyEqual(0+1.0, aPosX+1.0, SCROLL_BOUNDARY_EPSILON);
    mAtXEnd = (aPosX + (mContentResolution * mContentRect.width()) + SCROLL_BOUNDARY_EPSILON) >= mScrollableSize.width();
    mAtYBeginning = aPosY == 0 || gfx::FuzzyEqual(0+1.0, aPosY+1.0, SCROLL_BOUNDARY_EPSILON);
    mAtYEnd = (aPosY + (mContentResolution * mContentRect.height()) + SCROLL_BOUNDARY_EPSILON) >= mScrollableSize.height();
    if (oldAXB != mAtXBeginning) mViewIface->atXBeginningChanged();
    if (oldAXE != mAtXEnd)       mViewIface->atXEndChanged();
    if (oldAYB != mAtYBeginning) mViewIface->atYBeginningChanged();
    if (oldAYE != mAtYEnd)       mViewIface->atYEndChanged();

    if (widthChanged) {
        mViewIface->contentWidthChanged();
    }

    if (heightChanged) {
        mViewIface->contentHeightChanged();
    }

    if (widthChanged || heightChanged) {
        mViewIface->scrollableSizeChanged();
    }
}

void QMozViewPrivate::testFlickingMode(QTouchEvent *event)
{
    QTouchEvent::TouchPoint tp;
    QPointF touchPos;
    if (event->touchPoints().size() == 1) {
        tp = event->touchPoints().at(0);
        touchPos = tp.pos();
    }

    // Only for single press point
    if (!touchPos.isNull()) {
        if (event->type() == QEvent::TouchBegin) {
            mLastTimestamp = mLastStationaryTimestamp = current_timestamp(event);
            mCanFlick = true;
            mLastPos = QPointF();
            mSecondLastPos = QPointF();
        } else if (event->type() == QEvent::TouchUpdate && !mLastPos.isNull()) {
            QRectF pressArea = tp.rect();
            qreal touchHorizontalThreshold = pressArea.width() * 2;
            qreal touchVerticalThreshold = pressArea.height() * 2;
            if (!mLastStationaryPos.isNull() && (qAbs(mLastStationaryPos.x() - touchPos.x()) > touchHorizontalThreshold
                                                 || qAbs(mLastStationaryPos.y() - touchPos.y()) > touchVerticalThreshold)) {
                // Threshold exceeded. Reset stationary position and time.
                mLastStationaryTimestamp = current_timestamp(event);
                mLastStationaryPos = touchPos;
            } else if (qAbs(mLastPos.x() - touchPos.x()) <= touchHorizontalThreshold
                       && qAbs(mLastPos.y() - touchPos.y()) <= touchVerticalThreshold) {
                // Handle stationary position when panning stops and continues. Eventually mCanFlick is based on
                // timestamps + positions between events, see touch end block.
                if (mCanFlick) {
                    mLastStationaryTimestamp = current_timestamp(event);
                    mLastStationaryPos = touchPos;
                }
                mCanFlick = false;
            } else {
                mCanFlick = true;
            }
            mLastTimestamp = current_timestamp(event);
        } else if (event->type() == QEvent::TouchEnd) {
            // TouchBegin -> TouchEnd renders to state where we do not move. Take
            // that into account in mCanFlick. Evaluate movement from second last touch point
            // to avoid last update being at the touch end position. Ignore touch velocity and
            // use just flick threshold.
            bool hasMoved = false;
            if (!mSecondLastPos.isNull()) {
                hasMoved = !((tp.pos() - mSecondLastPos).isNull());
            }

            mCanFlick = (qint64(current_timestamp(event) - mLastTimestamp) < MOZVIEW_FLICK_THRESHOLD) &&
                    (qint64(current_timestamp(event) - mLastStationaryTimestamp) < MOZVIEW_FLICK_THRESHOLD) &&
                    hasMoved;
            mLastStationaryPos = QPointF();
        }
    }
    mLastPos = touchPos;
    mSecondLastPos = tp.lastPos();
}

void QMozViewPrivate::handleTouchEnd(bool &draggingChanged, bool &pinchingChanged)
{
    if (mDragging) {
        mDragging = false;
        draggingChanged = true;
    }

    // Currently change from 2> fingers to 1 finger does not
    // allow moving content. Hence, keep pinching enabled
    // also when there is one finger left when releasing
    // fingers and only stop pinching when touch ends.
    // You can continue pinching by adding second finger.
    if (mPinching) {
        mPinching = false;
        pinchingChanged = true;
    }
}

void QMozViewPrivate::resetTouchState()
{
    // Invalid initial drag start Y.
    mDragStartY = -1.0;
    mMoveDelta = 0.0;

    mFlicking = false;
    updateMoving(false);
    mVerticalScrollDecorator.setMoving(false);
    mHorizontalScrollDecorator.setMoving(false);
}

void QMozViewPrivate::updateMoving(bool moving)
{
    if (mMoving != moving) {
        mMoving = moving;

        if (mMoving && q) {
            startMoveMonitor();
        }
        mViewIface->movingChanged();
    }
}

void QMozViewPrivate::reset()
{
    if (mIsPainted) {
        mIsPainted = false;
        mViewIface->firstPaint(-1, -1);
    }

    if (mDOMContentLoaded) {
        mDOMContentLoaded = false;
        mViewIface->domContentLoadedChanged();
    }

    // In case we had dynamic toolbar height set, mark it dirty
    // to get it re-applied.
    if (mDynamicToolbarHeight > 0) {
        mDirtyState |= DirtyDynamicToolbarHeight;
    }
}

void QMozViewPrivate::setSize(const QSizeF &size)
{
    mSize = size;
    if (!mViewInitialized) {
        mDirtyState |= DirtySize;
    }
}

void QMozViewPrivate::setScreenProperties(int depth, qreal dpi)
{
    Q_ASSERT_X(mView, __PRETTY_FUNCTION__, "EmbedLiteView must be created by now");
    mDepth = depth;
    mDpi = dpi;
    if (!mHasCompositor) {
        mDirtyState |= DirtyScreenProperties;
    } else {
        mView->SetScreenProperties(mDepth, mDpi, mDpi);
    }
}

QUrl QMozViewPrivate::url() const
{
    return !mPendingUrl.isEmpty() ? QUrl(mPendingUrl) : QUrl(mUrl);
}

bool QMozViewPrivate::isUrlResolved() const
{
    return mPendingUrl.isEmpty() && !mUrl.isEmpty();
}

void QMozViewPrivate::goBack()
{
    if (!mViewInitialized)
        return;

    reset();
    mView->GoBack(false, true);
}

void QMozViewPrivate::goForward()
{
    if (!mViewInitialized)
        return;

    reset();
    mView->GoForward(false, true);
}

void QMozViewPrivate::stop()
{
    if (!mViewInitialized)
        return;
    mView->StopLoad();
}

void QMozViewPrivate::reload()
{
    if (!mViewInitialized)
        return;

    if (!mPendingUrl.isEmpty()) {
        load(mPendingUrl, mPendingFromExternal);
    } else {
        reset();
        mView->Reload(false);
    }
}

void QMozViewPrivate::load(const QString &url, const bool& fromExternal)
{
    if (url.isEmpty())
        return;

    if (!mViewInitialized) {
        mPendingUrl = url;
        mPendingFromExternal = fromExternal;
        mViewIface->urlChanged();
        return;
    }
#ifdef DEVELOPMENT_BUILD
    qCDebug(lcEmbedLiteExt) << "url:" << url.toUtf8().data();
#endif
    mProgress = 0;
    reset();
    mView->LoadURL(url.toUtf8().data(), fromExternal);

    if (mPendingUrl != url) {
        mPendingUrl = url;
        mPendingFromExternal = fromExternal;
        mViewIface->urlChanged();
    }
}

void QMozViewPrivate::scrollTo(int x, int y)
{
    if (mViewInitialized) {
        // Map to CSS pixels.
        mView->ScrollTo(x / mContentResolution, y / mContentResolution);
    }
}

void QMozViewPrivate::scrollBy(int x, int y)
{
    if (mViewInitialized) {
        // Map to CSS pixels.
        mView->ScrollBy(x / mContentResolution, y / mContentResolution);
    }
}

void QMozViewPrivate::runJavaScript(const QString &script, const QJSValue &callback, const QJSValue &errorCallback)
{
    if (!mViewInitialized) {
        auto viewInitialzedError = QStringLiteral("Error: run javascript can be called only after view is initialized.");
        if (errorCallback.isCallable()) {
            QJSValueList args = { QJSValue(viewInitialzedError) };
            // Make it possible to call const errorCallback.
            QJSValue cb = errorCallback;
            QJSValue result = cb.call(args);
            if (result.isError()) {
                qmlInfo(q) << viewInitialzedError;
            }
        } else {
            qmlInfo(q) << viewInitialzedError;
        }

        return;
    }

    // Callback undefined, fire and forget.
    if (callback.isUndefined()) {
        QVariantMap data;
        data.insert(QString("script"), script);
        data.insert(QString("callbackId"), -1);
        doSendAsyncMessage(QLatin1String(RUN_JAVASCRIPT), QVariant(data));
        return;
    }

    if (!callback.isCallable()) {
        auto callbackArgumentError = QStringLiteral("Error: callback argument is not a function.");

        if (errorCallback.isCallable()) {
            QJSValueList args = { QJSValue(callbackArgumentError) };
            // Make it possible to call const errorCallback.
            QJSValue cb = errorCallback;
            QJSValue result = cb.call(args);
            if (result.isError()) {
                qmlInfo(q) << callbackArgumentError;
            }
        } else {
            qmlInfo(q) << callbackArgumentError;
        }
        return;
    }

    if (!errorCallback.isUndefined() && !errorCallback.isCallable()) {
        qmlInfo(q) << "Error: error callback argument is not a function.";
        return;
    }

    uint callbackId = mNextJSCallId++;

    QVariantMap data;
    data.insert(QString("script"), script);
    data.insert(QString("callbackId"), callbackId);
    doSendAsyncMessage(QLatin1String(RUN_JAVASCRIPT), QVariant(data));

    mPendingJSCalls.insert(callbackId, qMakePair(callback, errorCallback));
}

bool QMozViewPrivate::domContentLoaded() const
{
    return mDOMContentLoaded;
}

void QMozViewPrivate::loadFrameScript(const QString &frameScript)
{
    if (!mViewInitialized) {
        mPendingFrameScripts.append(frameScript);
    } else {
        mView->LoadFrameScript(frameScript.toUtf8().data());
    }
}

void QMozViewPrivate::addMessageListener(const std::string &name)
{
    if (!mViewInitialized) {
        mPendingMessageListeners.push_back(name);
        return;
    }

    mView->AddMessageListener(name.c_str());
}

void QMozViewPrivate::addMessageListeners(const std::vector<std::string> &messageNamesList)
{
    if (!mViewInitialized) {
        mPendingMessageListeners.insert(mPendingMessageListeners.end(),
                                        messageNamesList.begin(),
                                        messageNamesList.end());
        return;
    }

    mView->AddMessageListeners(messageNamesList);
}

void QMozViewPrivate::timerEvent(QTimerEvent *event)
{
    Q_ASSERT(q);
    if (event->timerId() == mMovingTimerId) {
        qreal offsetY = mScrollableOffset.y();
        qreal offsetX = mScrollableOffset.x();
        if (offsetX == mOffsetX && offsetY == mOffsetY) {
            resetTouchState();
            q->killTimer(mMovingTimerId);
            mMovingTimerId = 0;
        }
        mOffsetX = offsetX;
        mOffsetY = offsetY;
        event->accept();
    }
}

void QMozViewPrivate::startMoveMonitor()
{
    Q_ASSERT(q);
    // Kill running move monitor.
    if (mMovingTimerId > 0) {
        q->killTimer(mMovingTimerId);;
        mMovingTimerId = 0;
    }
    mMovingTimerId = q->startTimer(MOZVIEW_FLICK_STOP_TIMEOUT);
    mFlicking = true;
}

QVariant QMozViewPrivate::inputMethodQuery(Qt::InputMethodQuery property) const
{
    switch (property) {
    case Qt::ImEnabled:
        return QVariant((bool) mIsInputFieldFocused);
    case Qt::ImHints:
        return QVariant((int) mInputMethodHints | (int) mInputMethodAttributes);
    case Qt::ImSurroundingText:
        return mSurroundingText;
    case Qt::ImCursorPosition:
        return mCursorPosition;
    case Qt::ImAnchorPosition:
        return mAnchorPosition;
    default:
        return QVariant();
    }
}

void QMozViewPrivate::inputMethodEvent(QInputMethodEvent *event)
{
#ifdef DEVELOPMENT_BUILD
    qCDebug(lcEmbedLiteExt) << "cStr:" << event->commitString().toUtf8().data()
                            << ", preStr:" << event->preeditString().toUtf8().data()
                            << ", replLen:" << event->replacementLength()
                            << ", replSt:" << event->replacementStart();
#endif

    if (mViewInitialized) {
        uint16_t charCode = (event->commitString().size() == 1 && event->commitString()[0].isPrint())
                          ? (int32_t)event->commitString()[0].unicode()
                          : 0;
        bool ok;
        int asciiNumber = event->commitString().toInt(&ok) + Qt::Key_0;
        if (ok && (mInputMethodHints & Qt::ImhFormattedNumbersOnly || mInputMethodHints & Qt::ImhDialableCharactersOnly)) {
            int32_t domKeyCode = MozKey::QtKeyCodeToDOMKeyCode(asciiNumber, Qt::NoModifier);
            mView->SendKeyPress(domKeyCode, 0, charCode);
            mView->SendKeyRelease(domKeyCode, 0, charCode);
            qGuiApp->inputMethod()->reset();

        } else {
            if (mPreedit || event->commitString().isEmpty() || event->commitString().size() > 1) {
                mView->SendTextEvent(event->commitString().toUtf8().data(), event->preeditString().toUtf8().data(),
                                     event->replacementStart(), event->replacementLength());
            } else {
                mView->SendKeyPress(0, 0, charCode);
                mView->SendKeyRelease(0, 0, charCode);
            }
        }
    }
    mPreedit = !event->preeditString().isEmpty();
}

void QMozViewPrivate::keyPressEvent(QKeyEvent *event)
{
    if (!mViewInitialized)
        return;

    int32_t gmodifiers = MozKey::QtModifierToDOMModifier(event->modifiers());
    int32_t domKeyCode = MozKey::QtKeyCodeToDOMKeyCode(event->key(), event->modifiers());
    int32_t charCode = 0;
    if (event->text().length() && event->text()[0].isPrint()) {
        charCode = (int32_t)event->text()[0].unicode();
        if (getenv("USE_TEXT_EVENTS")) {
            return;
        }
    }
    mView->SendKeyPress(domKeyCode, gmodifiers, charCode);
}

void QMozViewPrivate::keyReleaseEvent(QKeyEvent *event)
{
    if (!mViewInitialized)
        return;

    int32_t gmodifiers = MozKey::QtModifierToDOMModifier(event->modifiers());
    int32_t domKeyCode = MozKey::QtKeyCodeToDOMKeyCode(event->key(), event->modifiers());
    int32_t charCode = 0;
    if (event->text().length() && event->text()[0].isPrint()) {
        charCode = (int32_t)event->text()[0].unicode();
        if (getenv("USE_TEXT_EVENTS")) {
            mView->SendTextEvent(event->text().toUtf8().data(), "", 0, 0);
            return;
        }
    }
    mView->SendKeyRelease(domKeyCode, gmodifiers, charCode);
}

void QMozViewPrivate::sendAsyncMessage(const QString &message, const QVariant &value)
{
    if (!mViewInitialized)
        return;

    if (message == QLatin1String(RUN_JAVASCRIPT)) {
        qmlInfo(q) << "Error: trying to send reserved message:" << message;
        return;
    }

    doSendAsyncMessage(message, value);
}

void QMozViewPrivate::setMozWindow(QMozWindow *window)
{
    mMozWindow = window;
    if (mMozWindow) {
        mHasCompositor = mMozWindow->isCompositorCreated();
        connect(mMozWindow.data(), &QMozWindow::compositorCreated,
                this, &QMozViewPrivate::onCompositorCreated);
    }
}

void QMozViewPrivate::setParentId(unsigned parentId)
{
    if (parentId != mParentID) {
        mParentID = parentId;
        mViewIface->parentIdChanged();
    }
}

void QMozViewPrivate::setParentBrowsingContext(uintptr_t parentBrowsingContext)
{
    Q_ASSERT(mParentBrowsingContext == 0);
    mParentBrowsingContext = parentBrowsingContext;
}

void QMozViewPrivate::setHidden(bool hidden)
{
    mHidden = hidden;
}

void QMozViewPrivate::setChromeGestureEnabled(bool value)
{
    if (value != mChromeGestureEnabled) {
        mChromeGestureEnabled = value;
        mViewIface->chromeGestureEnabledChanged();
    }
}

void QMozViewPrivate::setChromeGestureThreshold(qreal value)
{
    if (value != mChromeGestureThreshold) {
        mChromeGestureThreshold = value;
        mViewIface->chromeGestureThresholdChanged();
    }
}

void QMozViewPrivate::setChrome(bool value)
{
    if (value != mChrome) {
        mChrome = value;
        mViewIface->chromeChanged();
    }
}

TouchPointF QMozViewPrivate::createEmbedTouchPoint(const QPointF &point) const
{
    return createEmbedTouchPoint(point.x(), point.y());
}

mozilla::embedlite::TouchPointF QMozViewPrivate::createEmbedTouchPoint(qreal posX, qreal posY) const
{
    QPointF offset = QPointF(0, 0);
    if (mDragging || mPinching) {
        // While dragging or pinching do not use rendering offset to avoid
        // unwanted jumping content.
        offset.setY(mTopMargin);
    } else {
        // precise coordinate
        offset = renderingOffset();
    }

    return mozilla::embedlite::TouchPointF(posX - offset.x(),
                                           posY - offset.y());
}

QPointF QMozViewPrivate::renderingOffset() const
{
    qreal y = mScrollableOffset.y() * QMozEngineSettings::instance()->pixelRatio();
    qreal dy = mTopMargin - std::min(mTopMargin, y);
    return QPointF(0.0f, std::max(qreal(0.0f), dy));
}

void QMozViewPrivate::onCompositorCreated()
{
    mHasCompositor = true;
    if (mDirtyState & DirtyScreenProperties) {
        mView->SetScreenProperties(mDepth, mDpi, mDpi);
        mDirtyState &= ~DirtyScreenProperties;
    }
}

void QMozViewPrivate::updateLoaded()
{
    bool loaded = mProgress == 100 && !mIsLoading;
    if (mLoaded != loaded) {
        mLoaded = loaded;

        // E.g. when loading images directly we don't necessarily get domContentLoaded message from engine.
        // So mark content loaded when webpage is loaded. This overloads "DOMContentLoaded" event a bit
        // also makes sure thata we eventually have DOM content loaded.
        if (mLoaded && !mDOMContentLoaded) {
            mDOMContentLoaded = true;
            mViewIface->domContentLoadedChanged();
        }

        clearDirtyDynamicToolbarHeight();
        mViewIface->loadedChanged();
    }
}

void QMozViewPrivate::createView()
{
    if (!mContext->isInitialized()) {
        connect(mContext, &QMozContext::initialized, this, &QMozViewPrivate::createView);
    } else {
        Q_ASSERT(!mView);
        QuickMozView *mozView = qobject_cast<QuickMozView*>(q);
        if (mozView) {
            mozView->prepareMozWindow();
        }

        Q_ASSERT(mMozWindow);

        EmbedLiteWindow *win = mMozWindow->d->mWindow;
        mView = mContext->GetApp()->CreateView(win, mParentID, mParentBrowsingContext, mPrivateMode, mDesktopMode);
        mView->SetListener(this);
        setScreenProperties(QGuiApplication::primaryScreen()->depth(),
                            QGuiApplication::primaryScreen()->physicalDotsPerInch());

        if (mozView) {
            connect(mMozWindow.data(), &QMozWindow::compositingFinished,
                    mozView, &QuickMozView::compositingFinished);
        }

        mViewIface->uniqueIdChanged();
    }
}

void QMozViewPrivate::ViewInitialized()
{
    mViewInitialized = true;

    // Load frame scripts first and then message listeners.
    Q_FOREACH (const QString &frameScript, mPendingFrameScripts) {
        loadFrameScript(frameScript);
    }
    mPendingFrameScripts.clear();

    addMessageListeners(mPendingMessageListeners);
    mPendingMessageListeners.clear();

    if (!mPendingUrl.isEmpty()) {
        load(mPendingUrl, mPendingFromExternal);
    }

    if (mDirtyState & DirtySize) {
        setSize(mSize);
        mDirtyState &= ~DirtySize;
    } else if (mMozWindow) {
        mSize = mMozWindow->size();
    }

    clearDirtyDynamicToolbarHeight();

    if (mDirtyState & DirtyMargin) {
        mView->SetMargins(mMargins.top(), mMargins.right(), mMargins.bottom(), mMargins.left());
        mViewIface->marginsChanged();
        mDirtyState &= ~DirtyMargin;
    }

    if (!mHttpUserAgent.isEmpty()) {
        mView->SetHttpUserAgent((const char16_t *)mHttpUserAgent.utf16());
    }

    // This is currently part of official API, so let's subscribe to these messages by default
    mViewIface->viewInitialized();
    mViewIface->canGoBackChanged();
    mViewIface->canGoForwardChanged();
}

void QMozViewPrivate::SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    mBackgroundColor = QColor(r, g, b, a);
    mViewIface->backgroundColorChanged();
}

void QMozViewPrivate::setDynamicToolbarHeight(const int height)
{
    if (height != mDynamicToolbarHeight) {
        mDynamicToolbarHeight = height;
        if (mViewInitialized && mDOMContentLoaded) {
            mView->SetDynamicToolbarHeight(height);
        } else {
            mDirtyState |= DirtyDynamicToolbarHeight;
        }
    }
}

void QMozViewPrivate::setMargins(const QMargins &margins, bool updateTopBottom)
{
    if (margins != mMargins) {
        mMargins = margins;

        if (updateTopBottom) {
            mTopMargin = mMargins.top();
            mBottomMargin = mMargins.bottom();
        }

        if (mViewInitialized) {
            mView->SetMargins(margins.top(), margins.right(), margins.bottom(), margins.left());
            mViewIface->marginsChanged();
        } else {
            mDirtyState |= DirtyMargin;
        }
    }
}

void QMozViewPrivate::OnLocationChanged(const char *aLocation, bool aCanGoBack, bool aCanGoForward)
{
    if (QLatin1String(aLocation) == QLatin1String("about:blank") && !aCanGoBack && !aCanGoForward) {
        return; // this is the preload location.  ignore it.
    }

    if (mCanGoBack != aCanGoBack) {
        mCanGoBack = aCanGoBack;
        mViewIface->canGoBackChanged();
    }

    if (mCanGoForward != aCanGoForward) {
        mCanGoForward = aCanGoForward;
        mViewIface->canGoForwardChanged();
    }

    mPendingUrl.clear();
    mPendingFromExternal = false;

    if (mUrl != aLocation) {
        mUrl = QString(aLocation);
        mViewIface->urlChanged();
    }
}

void QMozViewPrivate::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
    if (mIsLoading) {
        mProgress = aProgress;
        mViewIface->loadProgressChanged();
    }
}

void QMozViewPrivate::OnLoadStarted(const char *aLocation)
{
    Q_UNUSED(aLocation);

    reset();

    if (!mIsLoading) {
        mIsLoading = true;
        mProgress = 15;
        mViewIface->loadingChanged();
    }
    mSecurity.reset();
}

void QMozViewPrivate::OnLoadFinished(void)
{
    // if loading has stopped before location change, restore back
    // previous none empty url. Normally OnLocationChange clears pending url.
    if (!mPendingUrl.isEmpty() && !mUrl.isEmpty() && (mUrl != mPendingUrl)) {
        mPendingUrl.clear();
        mPendingFromExternal = false;
        mViewIface->urlChanged();
    }

    if (mIsLoading) {
        mProgress = 100;
        mIsLoading = false;
        mViewIface->loadingChanged();
    }
}

void QMozViewPrivate::OnWindowCloseRequested()
{
    mViewIface->windowCloseRequested();
}

// View finally destroyed and deleted
void QMozViewPrivate::ViewDestroyed()
{
    // TODO : Can this be removed?
    // Both ~QMozOpenGLWebPage and ~QuickMozView are
    // setting listener to null. Hence, EmbedLiteView::Destroyed()
    // will never be able to call this.

#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif

    mView = nullptr;
    mViewInitialized = false;

    if (mViewIface)
        mViewIface->viewDestroyed();
    mViewIface = nullptr;
}

void QMozViewPrivate::RecvAsyncMessage(const char16_t *aMessage, const char16_t *aData)
{
    QString message = QString::fromUtf16(aMessage);
    QString data = QString::fromUtf16(aData);

    bool ok = false;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);
    ok = error.error == QJsonParseError::NoError;
    QVariant vdata = doc.toVariant();

    if (ok) {
#ifdef DEVELOPMENT_BUILD
        qCDebug(lcEmbedLiteExt) << "mesg:" << message << ", data:" << data;
#endif
        if (!handleAsyncMessage(message, vdata))
            mViewIface->recvAsyncMessage(message, vdata);
    } else {
        qCWarning(lcEmbedLiteExt) << "JSON parse error:" << error.errorString();
#ifdef DEVELOPMENT_BUILD
        qCDebug(lcEmbedLiteExt) << "parse: s:'" << data << "', errLine:" << error.offset;
#endif
    }
}

char *QMozViewPrivate::RecvSyncMessage(const char16_t *aMessage, const char16_t *aData)
{
    QMozReturnValue response;

    QString message = QString::fromUtf16(aMessage);
    QString data = QString::fromUtf16(aData);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);
    Q_ASSERT(error.error == QJsonParseError::NoError);
    QVariant vdata = doc.toVariant();

    mViewIface->recvSyncMessage(message, vdata, &response);

    QVariant responseMessage = response.getMessage();
    QJsonDocument responseDocument;
    if (!responseMessage.isValid()) {
        // Default to an empty json
        responseDocument = QJsonDocument::fromJson("{\"\":\"\"}");
    } else if (responseMessage.userType() == QMetaType::type("QJSValue")) {
        // Qt 5.6 likes to pass a QJSValue
        QJSValue jsValue = qvariant_cast<QJSValue>(responseMessage);
        responseDocument = QJsonDocument::fromVariant(jsValue.toVariant());
    } else {
        responseDocument = QJsonDocument::fromVariant(responseMessage);
    }
    QByteArray array = responseDocument.toJson();
    return strdup(array.constData());
}

void QMozViewPrivate::OnLoadRedirect(void)
{
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    mViewIface->loadRedirect();
}

void QMozViewPrivate::OnSecurityChanged(const char *aStatus, unsigned int aState)
{
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    mSecurity.setSecurityRaw(aStatus, aState);
}

void QMozViewPrivate::OnFirstPaint(int32_t aX, int32_t aY)
{
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    mIsPainted = true;
    mViewIface->firstPaint(aX, aY);
}

void QMozViewPrivate::OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight)
{
    // Normally these come from HandleScrollEvent but for some documents no such event is generated.

    const float contentResoution = contentWindowSize(mMozWindow).width() / aWidth;

    if (!qFuzzyIsNull(contentResoution) && contentResoution != mContentResolution) {
        mContentResolution = contentResoution;
        mViewIface->resolutionChanged();
    }

    if (mContentRect.isEmpty()) {
        mContentRect.setSize(QSizeF(aWidth, aHeight));
    }

    updateScrollArea(aWidth * mContentResolution, aHeight * mContentResolution,
                     mScrollableOffset.x(), mScrollableOffset.y());
}

void QMozViewPrivate::setIsFocused(bool aIsFocused)
{
    mViewIsFocused = aIsFocused;
    if (mViewInitialized) {
        mView->SetIsFocused(aIsFocused);
    }
}

void QMozViewPrivate::setDesktopMode(bool aDesktopMode)
{
    if (mDesktopMode != aDesktopMode) {
        mDesktopMode = aDesktopMode;

        if (mViewInitialized) {
            mView->SetDesktopMode(aDesktopMode);
        }

        mViewIface->desktopModeChanged();
    }
}

void QMozViewPrivate::setThrottlePainting(bool aThrottle)
{
    if (mViewInitialized) {
        mView->SetThrottlePainting(aThrottle);
    }
}

void QMozViewPrivate::IMENotification(int aIstate, bool aOpen, int aCause, int aFocusChange,
                                      const char16_t *inputType, const char16_t *inputMode)
{
    Qt::InputMethodHints hints = Qt::ImhNone;
    hints = aIstate == 2 ? Qt::ImhHiddenText : Qt::ImhPreferLowercase;

    QString imType((QChar *)inputType);
    imType = imType.toLower();
    if (imType == QLatin1String("number")) {
        //hints |= Qt::ImhDigitsOnly;
        hints |= Qt::ImhFormattedNumbersOnly;
    } else if (imType == QLatin1String("tel")) {
        hints |= Qt::ImhDialableCharactersOnly;
    } else if (imType == QLatin1String("email")) {
        hints |= Qt::ImhEmailCharactersOnly;
    } else if (imType == QLatin1String("url")) {
        hints |= Qt::ImhUrlCharactersOnly;
    } else if (imType == QLatin1String("date")) {
        hints |= Qt::ImhDate;
    } else if (imType == QLatin1String("datetime") || imType == QLatin1String("datetime-local")) {
        hints |= Qt::ImhDate;
        hints |= Qt::ImhTime;
    } else if (imType == QLatin1String("time")) {
        hints |= Qt::ImhTime;
    }

    mViewIface->setInputMethodHints(hints);
    if (aFocusChange || aIstate) {
        mIsInputFieldFocused = aIstate;
        if (mViewIsFocused) {
#ifndef QT_NO_IM
            QInputMethod *inputContext = qGuiApp->inputMethod();
            if (!inputContext) {
                qCWarning(lcEmbedLiteExt) << "Requesting SIP: but no input context";
                return;
            }
            inputContext->update(Qt::ImEnabled);
            if (mIsInputFieldFocused) {
                inputContext->show();
            } else {
                inputContext->hide();
            }
            applyAutoCorrect();
#endif
        }
    }
    mViewIface->imeNotification(aIstate, aOpen, aCause, aFocusChange, imType);
}

void QMozViewPrivate::GetIMEStatus(int32_t *aIMEEnabled, int32_t *aIMEOpen)
{
    QInputMethod *inputContext = qGuiApp->inputMethod();
    if (aIMEOpen) {
        *aIMEOpen = inputContext->isVisible();
    }
}

void QMozViewPrivate::OnTitleChanged(const char16_t *aTitle)
{
    mTitle = QString((QChar *)aTitle);
    mViewIface->titleChanged();
}

void QMozViewPrivate::OnDynamicToolbarHeightChanged()
{
    mViewIface->dynamicToolbarHeightChanged();
}

bool QMozViewPrivate::HandleScrollEvent(const gfxRect &aContentRect, const gfxSize &aScrollableSize)
{
    if (mContentRect.x() != aContentRect.x || mContentRect.y() != aContentRect.y ||
            mContentRect.width() != aContentRect.width ||
            mContentRect.height() != aContentRect.height) {
        mContentRect.setRect(aContentRect.x, aContentRect.y, aContentRect.width, aContentRect.height);
        mViewIface->viewAreaChanged();
    }

    float contentResoution = contentWindowSize(mMozWindow).width() / aContentRect.width;
    if (!qFuzzyIsNull(contentResoution)) {
        if (mContentResolution != contentResoution) {
            mContentResolution = contentResoution;
            mViewIface->resolutionChanged();
        }
        updateScrollArea(
                    aScrollableSize.width * mContentResolution,
                    aScrollableSize.height * mContentResolution,
                    aContentRect.x * mContentResolution,
                    aContentRect.y * mContentResolution);
    }

    return false;
}

bool QMozViewPrivate::HandleLongTap(const nsIntPoint &aPoint)
{
    QMozReturnValue retval;
    retval.setMessage(false);
    mViewIface->handleLongTap(QPoint(aPoint.x, aPoint.y), &retval);
    return retval.getMessage().toBool();
}

bool QMozViewPrivate::HandleSingleTap(const nsIntPoint &aPoint)
{
    QMozReturnValue retval;
    retval.setMessage(false);
    mViewIface->handleSingleTap(QPoint(aPoint.x, aPoint.y), &retval);
    return retval.getMessage().toBool();
}

bool QMozViewPrivate::HandleDoubleTap(const nsIntPoint &aPoint)
{
    QMozReturnValue retval;
    retval.setMessage(false);
    mViewIface->handleDoubleTap(QPoint(aPoint.x, aPoint.y), &retval);
    return retval.getMessage().toBool();
}

void QMozViewPrivate::touchEvent(QTouchEvent *event)
{
    // QInputMethod sends the QInputMethodEvent. Thus, it will
    // be handled before this touch event. Problem is that
    // this also commits preedited text when moving web content.
    // This should be committed just before moving cursor position to
    // the old cursor position.
    if (mPreedit) {
        QInputMethod *inputContext = qGuiApp->inputMethod();
        if (inputContext) {
            inputContext->commit();
        }
        mPreedit = false;
    }

    // Always accept the QTouchEvent so that we'll receive also TouchUpdate and TouchEnd events
    mPendingTouchEvent = true;
    event->setAccepted(true);
    bool draggingChanged = false;
    bool pinchingChanged = false;
    bool testFlick = true;
    int touchPointsCount = event->touchPoints().size();

    if (event->type() == QEvent::TouchBegin) {
        Q_ASSERT(touchPointsCount > 0);
        mViewIface->forceViewActiveFocus();
        if (touchPointsCount > 1 && !mPinching) {
            mPinching = true;
            pinchingChanged = true;
        }
        resetTouchState();
    } else if (event->type() == QEvent::TouchUpdate) {
        Q_ASSERT(touchPointsCount > 0);
        if (!mDragging) {
            mDragging = true;
            mDragStartY = mContentRect.y() * mContentResolution;
            mMoveDelta = 0;
            draggingChanged = true;
        }

        if (touchPointsCount > 1 && !mPinching) {
            mPinching = true;
            pinchingChanged = true;
        }
    } else if (event->type() == QEvent::TouchEnd) {
        Q_ASSERT(touchPointsCount > 0);
        handleTouchEnd(draggingChanged, pinchingChanged);
    } else if (event->type() == QEvent::TouchCancel) {
        handleTouchEnd(draggingChanged, pinchingChanged);
        testFlick = false;
        mCanFlick = false;
    }

    if (testFlick) {
        testFlickingMode(event);
    }

    if (draggingChanged) {
        mViewIface->draggingChanged();
    }

    if (pinchingChanged) {
        mViewIface->pinchingChanged();
    }

    if (event->type() == QEvent::TouchEnd) {
        if (mCanFlick) {
            updateMoving(mCanFlick);
        } else {
            // From dragging (panning) end to clean state
            resetTouchState();
        }
    } else {
        updateMoving(mDragging);
    }

    qint64 timeStamp = current_timestamp(event);

    // Add active touch point to cancelled touch sequence.
    if (event->type() == QEvent::TouchCancel && touchPointsCount == 0) {
        QMapIterator<int, QPointF> i(mActiveTouchPoints);
        EmbedTouchInput touchEnd(EmbedTouchInput::MULTITOUCH_END, timeStamp);
        while (i.hasNext()) {
            i.next();
            QPointF pos = i.value();
            touchEnd.touches.push_back(TouchData(i.key(),
                                                 createEmbedTouchPoint(pos),
                                                 0));
        }
        // All touch point should be cleared but let's clear active touch points anyways.
        mActiveTouchPoints.clear();
        receiveInputEvent(touchEnd);
        // touch was canceled hence no need to generate touchstart or touchmove
        return;
    }

    QList<int> pressedIds, moveIds, endIds;
    QHash<int, int> idHash;
    for (int i = 0; i < touchPointsCount; ++i) {
        const QTouchEvent::TouchPoint &pt = event->touchPoints().at(i);
        idHash.insert(pt.id(), i);
        switch (pt.state()) {
        case Qt::TouchPointPressed: {
            mActiveTouchPoints.insert(pt.id(), pt.pos());
            pressedIds.append(pt.id());
            break;
        }
        case Qt::TouchPointReleased: {
            mActiveTouchPoints.remove(pt.id());
            endIds.append(pt.id());
            break;
        }
        case Qt::TouchPointMoved:
        case Qt::TouchPointStationary: {
            mActiveTouchPoints.insert(pt.id(), pt.pos());
            moveIds.append(pt.id());
            break;
        }
        default:
            break;
        }
    }

    // We should append previous touches to start event in order
    // to make Gecko recognize it as new added touches to existing session
    // and not evict it here http://hg.mozilla.org/mozilla-central/annotate/1d9c510b3742/layout/base/nsPresShell.cpp#l6135
    QList<int> startIds(moveIds);

    // Produce separate event for every pressed touch points
    Q_FOREACH (int id, pressedIds) {
        EmbedTouchInput touchStart(EmbedTouchInput::MULTITOUCH_START, timeStamp);
        startIds.append(id);
        std::sort(startIds.begin(), startIds.end(), std::less<int>());
        Q_FOREACH (int startId, startIds) {
            const QTouchEvent::TouchPoint &pt = event->touchPoints().at(idHash.value(startId));
            touchStart.touches.push_back(TouchData(pt.id(),
                                                   createEmbedTouchPoint(pt.pos()),
                                                   pt.pressure()));
        }

        receiveInputEvent(touchStart);
    }

    Q_FOREACH (int id, endIds) {
        const QTouchEvent::TouchPoint &pt = event->touchPoints().at(idHash.value(id));
        EmbedTouchInput touchEnd(EmbedTouchInput::MULTITOUCH_END, timeStamp);
        touchEnd.touches.push_back(TouchData(pt.id(),
                                             createEmbedTouchPoint(pt.pos()),
                                             pt.pressure()));
        receiveInputEvent(touchEnd);
    }

    if (!moveIds.empty()) {
        if (!pressedIds.empty()) {
            moveIds.append(pressedIds);
        }

        // Sort touch lists by IDs just in case JS code identifies touches
        // by their order rather than their IDs.
        std::sort(moveIds.begin(), moveIds.end(), std::less<int>());
        EmbedTouchInput touchMove(EmbedTouchInput::MULTITOUCH_MOVE, timeStamp);
        Q_FOREACH (int id, moveIds) {
            const QTouchEvent::TouchPoint &pt = event->touchPoints().at(idHash.value(id));
            touchMove.touches.push_back(TouchData(pt.id(),
                                                  createEmbedTouchPoint(pt.pos()),
                                                  pt.pressure()));
        }
        receiveInputEvent(touchMove);
    }
}

void QMozViewPrivate::receiveInputEvent(const EmbedTouchInput &event)
{
    if (mViewInitialized) {
        mView->ReceiveInputEvent(event);
    }
}

void QMozViewPrivate::synthTouchBegin(const QVariant &touches)
{
    QList<QVariant> list = touches.toList();
    EmbedTouchInput touchBegin(EmbedTouchInput::MULTITOUCH_START,
                               QDateTime::currentMSecsSinceEpoch());
    int ptId = 0;
    for (QList<QVariant>::iterator it = list.begin(); it != list.end(); it++) {
        const QPointF pt = (*it).toPointF();
        ptId++;
        touchBegin.touches.push_back(TouchData(ptId,
                                               createEmbedTouchPoint(pt),
                                               1.0f));
    }
    receiveInputEvent(touchBegin);
}

void QMozViewPrivate::synthTouchMove(const QVariant &touches)
{
    QList<QVariant> list = touches.toList();
    EmbedTouchInput touchMove(EmbedTouchInput::MULTITOUCH_MOVE,
                              QDateTime::currentMSecsSinceEpoch());
    int ptId = 0;
    for (QList<QVariant>::iterator it = list.begin(); it != list.end(); it++) {
        const QPointF pt = (*it).toPointF();
        ptId++;
        touchMove.touches.push_back(TouchData(ptId,
                                              createEmbedTouchPoint(pt),
                                              1.0f));
    }
    receiveInputEvent(touchMove);
}

void QMozViewPrivate::synthTouchEnd(const QVariant &touches)
{
    QList<QVariant> list = touches.toList();
    EmbedTouchInput touchEnd(EmbedTouchInput::MULTITOUCH_END,
                             QDateTime::currentMSecsSinceEpoch());
    int ptId = 0;
    for (QList<QVariant>::iterator it = list.begin(); it != list.end(); it++) {
        const QPointF pt = (*it).toPointF();
        ptId++;
        touchEnd.touches.push_back(TouchData(ptId,
                                             createEmbedTouchPoint(pt),
                                             1.0f));
    }
    receiveInputEvent(touchEnd);
}

void QMozViewPrivate::recvMouseMove(int posX, int posY)
{
    if (!mPendingTouchEvent) {
        EmbedTouchInput touchMove(EmbedTouchInput::MULTITOUCH_MOVE,
                                  QDateTime::currentMSecsSinceEpoch());
        touchMove.touches.push_back(TouchData(0,
                                              createEmbedTouchPoint(posX, posY),
                                              1.0f));
        receiveInputEvent(touchMove);
    }
}

void QMozViewPrivate::recvMousePress(int posX, int posY)
{
    mViewIface->forceViewActiveFocus();
    if (!mPendingTouchEvent) {
        EmbedTouchInput touchBegin(EmbedTouchInput::MULTITOUCH_START,
                                   QDateTime::currentMSecsSinceEpoch());
        touchBegin.touches.push_back(TouchData(0,
                                               createEmbedTouchPoint(posX, posY),
                                               1.0f));
        receiveInputEvent(touchBegin);
    }
}

void QMozViewPrivate::recvMouseRelease(int posX, int posY)
{
    if (!mPendingTouchEvent) {
        EmbedTouchInput touchEnd(EmbedTouchInput::MULTITOUCH_END,
                                 QDateTime::currentMSecsSinceEpoch());
        touchEnd.touches.push_back(TouchData(0,
                                             createEmbedTouchPoint(posX, posY),
                                             1.0f));
        receiveInputEvent(touchEnd);
    }
    if (mPendingTouchEvent) {
        mPendingTouchEvent = false;
    }
}

void QMozViewPrivate::doSendAsyncMessage(const QString &message, const QVariant &value)
{
    if (!mViewInitialized)
        return;

    QJsonDocument doc;
    if (value.userType() == QMetaType::type("QJSValue")) {
        // Qt 5.6 likes to pass a QJSValue
        QJSValue jsValue = qvariant_cast<QJSValue>(value);
        doc = QJsonDocument::fromVariant(jsValue.toVariant());
    } else {
        doc = QJsonDocument::fromVariant(value);
    }

    QByteArray array = doc.toJson(QJsonDocument::Compact);
    QString data(array);

    mView->SendAsyncMessage((const char16_t *)message.utf16(), (const char16_t *)data.utf16());
}

bool QMozViewPrivate::handleAsyncMessage(const QString &message, const QVariant &data)
{
    // Check docuri if this is an error page
    if (message == QLatin1String(CONTENT_LOADED)) {
        if (data.toMap().value(DOCURI_KEY).toString().startsWith(ABOUT_URL_PREFIX)) {
            // Mark security invalid, not used in error pages
            mSecurity.setSecurityRaw(nullptr, 0);
        }

        if (!mDOMContentLoaded) {
            mDOMContentLoaded = true;
            mViewIface->domContentLoadedChanged();
            clearDirtyDynamicToolbarHeight();
        }
        return false;
    } else if (message == QLatin1String(RUN_JAVASCRIPT_REPLY)) {
        QVariantMap map = data.toMap();
        uint jsCallId = map.value(QLatin1String("callbackId")).toUInt();
        QPair<QJSValue, QJSValue> callbacks = mPendingJSCalls.take(jsCallId);
        QVariant result = map.value(QLatin1String("result"));
        bool stringified = map.value(QLatin1String("stringified")).toBool();
        QVariant error = map.value(QLatin1String("error"));
        QJSValue callback = callbacks.first;
        if (error.isValid()) {
            QJSValue errorCallback = callbacks.second;
            if (errorCallback.isCallable()) {
                QJSValueList args = { QJSValue(error.toString()) };
                QJSValue result = errorCallback.call(args);
                if (result.isError()) {
                    qmlInfo(q) << "Error executing error callback";
                }
            } else {
                qmlInfo(q) << error;
            }
        } else if (callback.isCallable()) {
            // Over here callback should never be non-callable.
            QJSValueList args;
            if (stringified) {
                QJsonDocument doc = QJsonDocument::fromJson(result.toString().toUtf8());
                QVariant vdata = doc.toVariant();
                args = { callback.engine()->toScriptValue<QVariant>(vdata) };
            } else {
                args = { callback.engine()->toScriptValue<QVariant>(result) };
            }
            QJSValue result = callback.call(args);
            if (result.isError()) {
                qmlInfo(q) << "Error executing callback";
            }
        }

        return true;
    } else if (message == QLatin1String(FORMASSIST_RESULT)) {
        mAutoCompleteActive = true;
        mAutoCompleteList = data.toStringList();
        applyAutoCorrect();
        return true;
    } else if (message == QLatin1String(FORMASSIST_HIDE)) {
        mAutoCompleteActive = false;
        mAutoCompleteList.clear();
        applyAutoCorrect();
        return true;
    } else if (message == QLatin1String(INPUTMETHOD_SET_INPUT_CONTEXT)) {
        QVariantMap map = data.toMap();
        mSurroundingText = map.value(QLatin1String("surroundingText"));
        mCursorPosition = map.value(QLatin1String("cursorPosition"));
        mAnchorPosition = map.value(QLatin1String("anchorPosition"));
        QInputMethod *inputContext = qGuiApp->inputMethod();
        inputContext->update(Qt::ImSurroundingText | Qt::ImCursorPosition | Qt::ImAnchorPosition);
        return true;
    } else if (message == QLatin1String(INPUTMETHOD_RESET_INPUT_CONTEXT)) {
        mSurroundingText = QVariant();
        mCursorPosition = QVariant();
        mAnchorPosition = QVariant();
        QInputMethod *inputContext = qGuiApp->inputMethod();
        inputContext->update(Qt::ImSurroundingText | Qt::ImCursorPosition | Qt::ImAnchorPosition);
        return true;
    } else if (message == QLatin1String(INPUTMETHOD_SET_INPUT_ATTRIBUTES)) {
        QVariantMap map = data.toMap();
        mInputMethodAttributes = 0;
        if (map.value(QLatin1String("autocomplete")) == QLatin1String("off")) {
            mInputMethodAttributes |= Qt::ImhNoPredictiveText | Qt::ImhSensitiveData;
        }
        if (map.value(QLatin1String("autocapitalize")) == QLatin1String("off") ||
                map.value(QLatin1String("autocapitalize")) == QLatin1String("none")) {
            mInputMethodAttributes |= Qt::ImhNoAutoUppercase | Qt::ImhPreferLowercase;
        } else if (map.value(QLatin1String("autocapitalize")) == QLatin1String("characters")) {
            mInputMethodAttributes |= Qt::ImhPreferUppercase;
        }
        qGuiApp->inputMethod()->update(Qt::ImHints);
        return true;
    } else if (message == QLatin1String(INPUTMETHOD_RESET_INPUT_ATTRIBUTES)) {
        mInputMethodAttributes = 0;
        qGuiApp->inputMethod()->update(Qt::ImHints);
        return true;
    }

    return false;
}

void QMozViewPrivate::clearDirtyDynamicToolbarHeight()
{
    if ((mDirtyState & DirtyDynamicToolbarHeight) && mViewInitialized && mDOMContentLoaded) {
        mView->SetDynamicToolbarHeight(mDynamicToolbarHeight);
        mDirtyState &= ~DirtyDynamicToolbarHeight;
    }
}

void QMozViewPrivate::applyAutoCorrect()
{
    QInputMethod *inputContext = qGuiApp->inputMethod();
    QVariantMap extensions = q->property("__inputMethodExtensions").toMap();

    if (mAutoCompleteActive && mViewIsFocused) {
        extensions.insert(QStringLiteral("autoFillSuggestions"), mAutoCompleteList);
        extensions.insert(QStringLiteral("autoFillCanRemove"), false);
        q->setProperty("__inputMethodExtensions", extensions);
        inputContext->update(Qt::ImPlatformData);
    } else {
        extensions.remove(QStringLiteral("autoFillSuggestions"));
        extensions.remove(QStringLiteral("autoFillCanRemove"));
        q->setProperty("__inputMethodExtensions", extensions);
        inputContext->update(Qt::ImQueryAll);
    }
}

void QMozViewPrivate::setHttpUserAgent(const QString &httpUserAgent)
{
    if (mHttpUserAgent != httpUserAgent) {
        mHttpUserAgent = httpUserAgent;
        if (mViewInitialized && mView) {
            mView->SetHttpUserAgent((const char16_t *)mHttpUserAgent.utf16());
        }
        mViewIface->httpUserAgentChanged();
    }
}

QString QMozViewPrivate::httpUserAgent() const
{
    return mHttpUserAgent;
}

void QMozViewPrivate::OnHttpUserAgentUsed(const char16_t *aHttpUserAgent)
{
    QString httpUserAgent = QString::fromUtf16(aHttpUserAgent);
    if (mHttpUserAgent != httpUserAgent) {
        // Update the property, but don't send it back to the engine
        mHttpUserAgent = httpUserAgent;
        mViewIface->httpUserAgentChanged();
    }
}
