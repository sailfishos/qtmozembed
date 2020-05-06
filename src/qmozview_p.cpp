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
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTouchEvent>

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
#define DOCURI_KEY "docuri"
#define ABOUT_URL_PREFIX "about:"

qint64 current_timestamp(QTouchEvent *aEvent)
{
    if (aEvent) {
        return aEvent->timestamp();
    }

    struct timeval te;
    gettimeofday(&te, NULL);
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
    , mMozWindow(NULL)
    , mContext(NULL)
    , mView(NULL)
    , mViewInitialized(false)
    , mBgColor(Qt::white)
    , mTopMargin(0.0)
    , mBottomMargin(0.0)
    , mTempTexture(NULL)
    , mEnabled(true)
    , mChromeGestureEnabled(true)
    , mChromeGestureThreshold(0.0)
    , mChrome(true)
    , mMoveDelta(0.0)
    , mDragStartY(0.0)
    , mMoving(false)
    , mPinching(false)
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
    , mContentResolution(0.0)
    , mIsPainted(false)
    , mInputMethodHints(0)
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
    , mDirtyState(0)
{
}

QMozViewPrivate::~QMozViewPrivate()
{
    delete mViewIface;
}

void QMozViewPrivate::UpdateScrollArea(unsigned int aWidth, unsigned int aHeight, float aPosX, float aPosY)
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
}

void QMozViewPrivate::TestFlickingMode(QTouchEvent *event)
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
                // Handle stationary position when panning stops and continues. Eventually mCanFlick is based on timestamps + positions between events, see touch end block.
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

void QMozViewPrivate::HandleTouchEnd(bool &draggingChanged, bool &pinchingChanged)
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

void QMozViewPrivate::ResetState()
{
    // Invalid initial drag start Y.
    mDragStartY = -1.0;
    mMoveDelta = 0.0;

    mFlicking = false;
    UpdateMoving(false);
    mVerticalScrollDecorator.setMoving(false);
    mHorizontalScrollDecorator.setMoving(false);
}

void QMozViewPrivate::UpdateMoving(bool moving)
{
    if (mMoving != moving) {
        mMoving = moving;

        if (mMoving && q) {
            startMoveMonitor();
        }
        mViewIface->movingChanged();
    }
}

void QMozViewPrivate::ResetPainted()
{
    if (mIsPainted) {
        mIsPainted = false;
        mViewIface->firstPaint(-1, -1);
    }
}

void QMozViewPrivate::setSize(const QSizeF &size)
{
    mSize = size;
    if (!mViewInitialized) {
        mDirtyState |= DirtySize;
    }
}

void QMozViewPrivate::load(const QString &url)
{
    if (url.isEmpty())
        return;

    if (!mViewInitialized) {
        mPendingUrl = url;
        return;
    }
#ifdef DEVELOPMENT_BUILD
    qCDebug(lcEmbedLiteExt) << "url:" << url.toUtf8().data();
#endif
    mProgress = 0;
    ResetPainted();
    mView->LoadURL(url.toUtf8().data());
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

void QMozViewPrivate::loadFrameScript(const QString &frameScript)
{
    if (!mViewInitialized) {
        mPendingFrameScripts.append(frameScript);
    } else {
        mView->LoadFrameScript(frameScript.toUtf8().data());
    }
}

void QMozViewPrivate::addMessageListener(const QString &name)
{
    if (!mViewInitialized) {
        mPendingMessageListeners.append(name);
        return;
    }

    mView->AddMessageListener(name.toUtf8().data());
}

void QMozViewPrivate::addMessageListeners(const QStringList &messageNamesList)
{
    if (!mViewInitialized) {
        mPendingMessageListeners.append(messageNamesList);
        return;
    }

    nsTArray<nsString> messages;
    for (int i = 0; i < messageNamesList.size(); i++) {
        messages.AppendElement((char16_t *)messageNamesList.at(i).data());
    }
    mView->AddMessageListeners(messages);
}

void QMozViewPrivate::timerEvent(QTimerEvent *event)
{
    Q_ASSERT(q);
    if (event->timerId() == mMovingTimerId) {
        qreal offsetY = mScrollableOffset.y();
        qreal offsetX = mScrollableOffset.x();
        if (offsetX == mOffsetX && offsetY == mOffsetY) {
            ResetState();
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
        return QVariant((int) mInputMethodHints);
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

    mPreedit = !event->preeditString().isEmpty();
    if (mViewInitialized) {
        if (mInputMethodHints & Qt::ImhFormattedNumbersOnly || mInputMethodHints & Qt::ImhDialableCharactersOnly) {
            bool ok;
            int asciiNumber = event->commitString().toInt(&ok) + Qt::Key_0;

            if (ok) {
                int32_t domKeyCode = MozKey::QtKeyCodeToDOMKeyCode(asciiNumber, Qt::NoModifier);
                int32_t charCode = 0;

                if (event->commitString().length() && event->commitString()[0].isPrint()) {
                    charCode = (int32_t)event->commitString()[0].unicode();
                }
                mView->SendKeyPress(domKeyCode, 0, charCode);
                mView->SendKeyRelease(domKeyCode, 0, charCode);
                qGuiApp->inputMethod()->reset();
            } else {
                mView->SendTextEvent(event->commitString().toUtf8().data(), event->preeditString().toUtf8().data());
            }
        } else {
            if (event->commitString().isEmpty()) {
                mView->SendTextEvent(event->commitString().toUtf8().data(), event->preeditString().toUtf8().data());
            } else {
                mView->SendTextEvent(event->commitString().toUtf8().data(), event->preeditString().toUtf8().data());
                // After commiting pre-edit, we send "dummy" keypress.
                // Workaround for sites that enable "submit" button based on keypress events like
                // comment fields in FB, and m.linkedin.com
                // Chrome on Android does the same, but it does it also after each pre-edit change
                // We cannot do exectly the same here since sending keyevent with active pre-edit would commit gecko's
                // internal Input Engine's pre-edit
                mView->SendKeyPress(0, 0, 0);
                mView->SendKeyRelease(0, 0, 0);
            }
        }
    }
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
            mView->SendTextEvent(event->text().toUtf8().data(), "");
            return;
        }
    }
    mView->SendKeyRelease(domKeyCode, gmodifiers, charCode);
}

void QMozViewPrivate::sendAsyncMessage(const QString &name, const QVariant &value)
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
    QByteArray array = doc.toJson();
    mView->SendAsyncMessage((const char16_t *)name.constData(), NS_ConvertUTF8toUTF16(array.constData()).get());
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

    return mozilla::embedlite::TouchPointF((posX - offset.x())/QMozEngineSettings::instance()->pixelRatio(),
                                           (posY - offset.y())/QMozEngineSettings::instance()->pixelRatio());
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
}

void QMozViewPrivate::ViewInitialized()
{
    mViewInitialized = true;

    Q_FOREACH (const QString &listener, mPendingMessageListeners) {
        addMessageListener(listener);
    }
    mPendingMessageListeners.clear();

    Q_FOREACH (const QString &frameScript, mPendingFrameScripts) {
        loadFrameScript(frameScript);
    }
    mPendingFrameScripts.clear();

    if (!mPendingUrl.isEmpty()) {
        load(mPendingUrl);
        mPendingUrl.clear();
    }

    if (mDirtyState & DirtySize) {
        setSize(mSize);
        mDirtyState &= ~DirtySize;
    } else if (mMozWindow) {
        mSize = mMozWindow->size();
    }

    if (mDirtyState & DirtyMargin) {
        mView->SetMargins(mMargins.top(), mMargins.right(), mMargins.bottom(), mMargins.left());
        mViewIface->marginsChanged();
        mDirtyState &= ~DirtyMargin;
    }

    // This is currently part of official API, so let's subscribe to these messages by default
    mViewIface->viewInitialized();
    mViewIface->canGoBackChanged();
    mViewIface->canGoForwardChanged();
}

void QMozViewPrivate::SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    QMutexLocker locker(&mBgColorMutex);
    mBgColor = QColor(r, g, b, a);
    mViewIface->bgColorChanged();
}

void QMozViewPrivate::SetMargins(const QMargins &margins, bool updateTopBottom)
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

// Can be read for instance from gecko compositor thread.
QColor QMozViewPrivate::GetBackgroundColor() const
{
    QMutexLocker locker(&mBgColorMutex);
    return mBgColor;
}

void QMozViewPrivate::OnLocationChanged(const char *aLocation, bool aCanGoBack, bool aCanGoForward)
{
    if (mCanGoBack != aCanGoBack) {
        mCanGoBack = aCanGoBack;
        mViewIface->canGoBackChanged();
    }

    if (mCanGoForward != aCanGoForward) {
        mCanGoForward = aCanGoForward;
        mViewIface->canGoForwardChanged();
    }

    if (mLocation != aLocation) {
        mLocation = QString(aLocation);
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

    ResetPainted();

    if (!mIsLoading) {
        mIsLoading = true;
        mProgress = 1;
        mViewIface->loadingChanged();
    }
    mSecurity.reset();
}

void QMozViewPrivate::OnLoadFinished(void)
{
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
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    mView = NULL;
    mViewInitialized = false;
    mViewIface->viewDestroyed();
}

void QMozViewPrivate::RecvAsyncMessage(const char16_t *aMessage, const char16_t *aData)
{
    NS_ConvertUTF16toUTF8 message(aMessage);
    NS_ConvertUTF16toUTF8 data(aData);

    bool ok = false;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(data.get()), &error);
    ok = error.error == QJsonParseError::NoError;
    QVariant vdata = doc.toVariant();

    // Check docuri if this is an error page
    if (message == CONTENT_LOADED && vdata.toMap().value(DOCURI_KEY).toString().startsWith(ABOUT_URL_PREFIX)) {
        // Mark security invalid, not used in error pages
        mSecurity.setSecurityRaw(NULL, 0);
    }

    if (ok) {
#ifdef DEVELOPMENT_BUILD
        qCDebug(lcEmbedLiteExt) << "mesg:" << message.get() << ", data:" << data.get();
#endif
        mViewIface->recvAsyncMessage(message.get(), vdata);
    } else {
        qCWarning(lcEmbedLiteExt) << "JSON parse error:" << error.errorString().toUtf8().data();
#ifdef DEVELOPMENT_BUILD
        qCDebug(lcEmbedLiteExt) << "parse: s:'" << data.get() << "', errLine:" << error.offset;
#endif
    }
}

char *QMozViewPrivate::RecvSyncMessage(const char16_t *aMessage, const char16_t *aData)
{
    QMozReturnValue response;
    NS_ConvertUTF16toUTF8 message(aMessage);
    NS_ConvertUTF16toUTF8 data(aData);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(data.get()), &error);
    Q_ASSERT(error.error == QJsonParseError::NoError);
    QVariant vdata = doc.toVariant();

    mViewIface->recvSyncMessage(message.get(), vdata, &response);

    QVariant responseMessage = response.getMessage();
    QJsonDocument responseDocument;
    if (responseMessage.userType() == QMetaType::type("QJSValue")) {
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
    if (!qFuzzyIsNull(contentResoution)) {
        mContentResolution = contentResoution;
    }

    if (mContentRect.isEmpty()) {
        mContentRect.setSize(QSizeF(aWidth, aHeight));
    }

    UpdateScrollArea(aWidth * mContentResolution, aHeight * mContentResolution,
                     mScrollableOffset.x(), mScrollableOffset.y());
}

void QMozViewPrivate::SetIsFocused(bool aIsFocused)
{
    mViewIsFocused = aIsFocused;
    if (mViewInitialized) {
        mView->SetIsFocused(aIsFocused);
    }
}

void QMozViewPrivate::SetThrottlePainting(bool aThrottle)
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
            inputContext->update(Qt::ImQueryAll);
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

bool QMozViewPrivate::HandleScrollEvent(bool aIsRootScrollFrame, const gfxRect &aContentRect, const gfxSize &aScrollableSize)
{
    // aIsRootScrollFrame makes it possible to handle chrome gesture also in case that we have
    // an iframe that is of the size of the screen. We may need to add still a scrollable layer id or similar.
    if (!aIsRootScrollFrame)
        return false;

    if (mContentRect.x() != aContentRect.x || mContentRect.y() != aContentRect.y ||
            mContentRect.width() != aContentRect.width ||
            mContentRect.height() != aContentRect.height) {
        mContentRect.setRect(aContentRect.x, aContentRect.y, aContentRect.width, aContentRect.height);
        mViewIface->viewAreaChanged();
    }

    float contentResoution = contentWindowSize(mMozWindow).width() / aContentRect.width;
    if (!qFuzzyIsNull(contentResoution)) {
        mContentResolution = contentResoution;
        UpdateScrollArea(
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
        ResetState();
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
        HandleTouchEnd(draggingChanged, pinchingChanged);
    } else if (event->type() == QEvent::TouchCancel) {
        HandleTouchEnd(draggingChanged, pinchingChanged);
        testFlick = false;
        mCanFlick = false;
    }

    if (testFlick) {
        TestFlickingMode(event);
    }

    if (draggingChanged) {
        mViewIface->draggingChanged();
    }

    if (pinchingChanged) {
        mViewIface->pinchingChanged();
    }

    if (event->type() == QEvent::TouchEnd) {
        if (mCanFlick) {
            UpdateMoving(mCanFlick);
        } else {
            // From dragging (panning) end to clean state
            ResetState();
        }
    } else {
        UpdateMoving(mDragging);
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
        ReceiveInputEvent(touchEnd);
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

        ReceiveInputEvent(touchStart);
    }

    Q_FOREACH (int id, endIds) {
        const QTouchEvent::TouchPoint &pt = event->touchPoints().at(idHash.value(id));
        EmbedTouchInput touchEnd(EmbedTouchInput::MULTITOUCH_END, timeStamp);
        touchEnd.touches.push_back(TouchData(pt.id(),
                                             createEmbedTouchPoint(pt.pos()),
                                             pt.pressure()));
        ReceiveInputEvent(touchEnd);
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
        ReceiveInputEvent(touchMove);
    }
}

void QMozViewPrivate::ReceiveInputEvent(const EmbedTouchInput &event)
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
    ReceiveInputEvent(touchBegin);
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
    ReceiveInputEvent(touchMove);
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
    ReceiveInputEvent(touchEnd);
}

void QMozViewPrivate::recvMouseMove(int posX, int posY)
{
    if (!mPendingTouchEvent) {
        EmbedTouchInput touchMove(EmbedTouchInput::MULTITOUCH_MOVE,
                                  QDateTime::currentMSecsSinceEpoch());
        touchMove.touches.push_back(TouchData(0,
                                              createEmbedTouchPoint(posX, posY),
                                              1.0f));
        ReceiveInputEvent(touchMove);
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
        ReceiveInputEvent(touchBegin);
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
        ReceiveInputEvent(touchEnd);
    }
    if (mPendingTouchEvent) {
        mPendingTouchEvent = false;
    }
}
