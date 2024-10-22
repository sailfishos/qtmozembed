/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozopenglwebpage.h"

#include <qglobal.h>
#include <qqmlinfo.h>

#include "mozilla/embedlite/EmbedLiteApp.h"

#include "qmozview_p.h"
#include "qmozcontext.h"
#include "qmozembedlog.h"
#include "qmozgrabresult.h"
#include "qmozscrolldecorator.h"
#include "qmozwindow.h"
#include "qmozwindow_p.h"

#define LOG_COMPONENT "QMozOpenGLWebPage"

/*!
    \fn void QMozOpenGLWebPage::afterRendering()

    This signal is emitted after web content has been rendered, before swapbuffers
    has been called.

    This signal can be used to paint using raw GL on top of the web content, or to do
    screen scraping of the current frame buffer.

    The GL context used for rendering is bound at this point.

    This signal is emitted from the gecko compositor thread, you must make sure that
    the connection is direct (see Qt::ConnectionType) in case you are about to paint
    using raw GL.
*/

/*!
    \fn void QMozOpenGLWebPage::QMozOpenGLWebPage(QObject *parent)

    In order to use this, embedlite.compositor.external_gl_context preference needs to be set.
*/
QMozOpenGLWebPage::QMozOpenGLWebPage(QObject *parent)
    : QObject(parent)
    , d(new QMozViewPrivate(new IMozQView<QMozOpenGLWebPage>(*this), this))
    , mCompleted(false)
    , mSizeUpdateScheduled(false)
    , mThrottlePainting(false)
    , m_virtualKeyboardHeight(0)
{
    d->mContext = QMozContext::instance();

    connect(this, &QMozOpenGLWebPage::viewInitialized, this, &QMozOpenGLWebPage::processViewInitialization);
    connect(this, &QMozOpenGLWebPage::loadProgressChanged, d, &QMozViewPrivate::updateLoaded);
    connect(this, &QMozOpenGLWebPage::loadingChanged, d, &QMozViewPrivate::updateLoaded);
}

QMozOpenGLWebPage::~QMozOpenGLWebPage()
{
    if (d->mView) {
        d->mView->SetIsActive(false);
        d->mView->SetListener(nullptr);
        d->mContext->GetApp()->DestroyView(d->mView);
    }
    QMutexLocker lock(&mGrabResultListLock);
    mGrabResultList.clear();
    delete d;
    d = nullptr;
}

void QMozOpenGLWebPage::processViewInitialization()
{
    // This is connected to view initialization. View must be initialized
    // over here.
    Q_ASSERT(d->mViewInitialized);

    mCompleted = true;
    setActive(true);
    Q_EMIT completedChanged();
}

void QMozOpenGLWebPage::onDrawOverlay(const QRect &rect)
{
    {
        QMutexLocker lock(&mGrabResultListLock);
        QList<QWeakPointer<QMozGrabResult> >::const_iterator it = mGrabResultList.begin();
        for (; it != mGrabResultList.end(); ++it) {
            QSharedPointer<QMozGrabResult> result = it->toStrongRef();
            if (result) {
                result->captureImage(rect);
            } else {
                qWarning() << "QMozGrabResult freed before being realized!";
            }
        }
        mGrabResultList.clear();
    }
    Q_EMIT afterRendering();
}

int QMozOpenGLWebPage::parentId() const
{
    return d->mParentID;
}

bool QMozOpenGLWebPage::privateMode() const
{
    return d->mPrivateMode;
}

void QMozOpenGLWebPage::setPrivateMode(bool privateMode)
{
    if (d->mView) {
        // View is created directly in componentComplete() if mozcontext ready
        qmlInfo(this) << "privateMode cannot be changed after view is created";
        return;
    }

    if (privateMode != d->mPrivateMode) {
        d->mPrivateMode = privateMode;
        Q_EMIT privateModeChanged();
    }
}

bool QMozOpenGLWebPage::enabled() const
{
    return d->mEnabled;
}

void QMozOpenGLWebPage::setEnabled(bool enabled)
{
    if (d->mEnabled != enabled) {
        d->mEnabled = enabled;
        Q_EMIT enabledChanged();
    }
}

bool QMozOpenGLWebPage::active() const
{
    return d->mActive;
}

void QMozOpenGLWebPage::setActive(bool active)
{
    // WebPage is in inactive state until the view is initialized.
    // ::processViewInitialization always forces active state so we
    // can just ignore early activation calls.
    if (!d || !d->mViewInitialized)
        return;

    if (d->mActive != active) {
        d->mActive = active;
        d->mView->SetIsActive(d->mActive);
        Q_EMIT activeChanged();
    }
}

bool QMozOpenGLWebPage::loaded() const
{
    return d->mLoaded;
}

QMozWindow *QMozOpenGLWebPage::mozWindow() const
{
    return d->mMozWindow;
}

void QMozOpenGLWebPage::setMozWindow(QMozWindow *window)
{
    d->setMozWindow(window);

    if (window) {
        window->setSize(d->mSize.toSize());
    }

    connect(window, &QMozWindow::drawOverlay,
            this, &QMozOpenGLWebPage::onDrawOverlay, Qt::DirectConnection);
}

bool QMozOpenGLWebPage::desktopMode() const
{
    return d->mDesktopMode;
}

void QMozOpenGLWebPage::setDesktopMode(bool desktopMode)
{
    d->setDesktopMode(desktopMode);
}

bool QMozOpenGLWebPage::throttlePainting() const
{
    return mThrottlePainting;
}

void QMozOpenGLWebPage::setThrottlePainting(bool throttle)
{
    if (mThrottlePainting != throttle) {
        mThrottlePainting = throttle;
        d->setThrottlePainting(throttle);
        Q_EMIT throttlePaintingChanged();
    }
}

int QMozOpenGLWebPage::virtualKeyboardHeight() const
{
    return m_virtualKeyboardHeight;
}

void QMozOpenGLWebPage::setVirtualKeyboardHeight(int height)
{
    if (height != m_virtualKeyboardHeight) {
        m_virtualKeyboardHeight = height;
        Q_EMIT virtualKeyboardHeightChanged();
    }
}

/*!
    \fn void QMozOpenGLWebPage::initialize()

    Call initialize to complete web page creation.
*/
void QMozOpenGLWebPage::initialize()
{
    d->createView();
}

bool QMozOpenGLWebPage::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::InputMethodQuery: {
        QInputMethodQueryEvent *query = static_cast<QInputMethodQueryEvent *>(event);
        Qt::InputMethodQueries queries = query->queries();
        for (int bit = 0; bit < 32; bit++) {
            Qt::InputMethodQuery q = (Qt::InputMethodQuery)(1 << bit);
            if (queries & q) query->setValue(q, inputMethodQuery(q));
        }
        query->accept();
        break;
    }
    case QEvent::InputMethod:
        inputMethodEvent(static_cast<QInputMethodEvent *>(event));
        break;
    case QEvent::FocusIn:
        focusInEvent(static_cast<QFocusEvent *>(event));
        break;
    case QEvent::FocusOut:
        focusOutEvent(static_cast<QFocusEvent *>(event));
        break;
    case QEvent::KeyPress:
        keyPressEvent(static_cast<QKeyEvent *>(event));
        break;
    case QEvent::KeyRelease:
        keyReleaseEvent(static_cast<QKeyEvent *>(event));
        break;

    default:
        return QObject::event(event);
    }
    return true;
}

bool QMozOpenGLWebPage::completed() const
{
    return mCompleted;
}

void QMozOpenGLWebPage::update()
{
    if (!d->mViewInitialized) {
        return;
    }

    d->mView->ScheduleUpdate();
}

void QMozOpenGLWebPage::forceActiveFocus()
{
    if (!d->mViewInitialized) {
        return;
    }

    setActive(true);
    d->setIsFocused(true);
}

void QMozOpenGLWebPage::setInputMethodHints(Qt::InputMethodHints hints)
{
    d->mInputMethodHints = hints;
}

void QMozOpenGLWebPage::inputMethodEvent(QInputMethodEvent *event)
{
    d->inputMethodEvent(event);
}

void QMozOpenGLWebPage::keyPressEvent(QKeyEvent *event)
{
    d->keyPressEvent(event);
}

void QMozOpenGLWebPage::keyReleaseEvent(QKeyEvent *event)
{
    return d->keyReleaseEvent(event);
}

QVariant QMozOpenGLWebPage::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return d->inputMethodQuery(property);
}

void QMozOpenGLWebPage::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    d->setIsFocused(true);
}

void QMozOpenGLWebPage::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    d->setIsFocused(false);
}

void QMozOpenGLWebPage::forceViewActiveFocus()
{
    forceActiveFocus();
}

QUrl QMozOpenGLWebPage::url() const
{
    return d->url();
}

void QMozOpenGLWebPage::setUrl(const QUrl &url)
{
    load(url.toString(), false);
}

bool QMozOpenGLWebPage::isUrlResolved() const
{
    return d->isUrlResolved();
}

QString QMozOpenGLWebPage::title() const
{
    return d->mTitle;
}

int QMozOpenGLWebPage::loadProgress() const
{
    return d->mProgress;
}

bool QMozOpenGLWebPage::canGoBack() const
{
    return d->mCanGoBack;
}

bool QMozOpenGLWebPage::canGoForward() const
{
    return d->mCanGoForward;
}

bool QMozOpenGLWebPage::loading() const
{
    return d->mIsLoading;
}

QRectF QMozOpenGLWebPage::contentRect() const
{
    return d->mContentRect;
}

QSizeF QMozOpenGLWebPage::scrollableSize() const
{
    return d->mScrollableSize;
}

QPointF QMozOpenGLWebPage::scrollableOffset() const
{
    return d->mScrollableOffset;
}

bool QMozOpenGLWebPage::atXBeginning() const
{
    return d->mAtXBeginning;
}

bool QMozOpenGLWebPage::atXEnd() const
{
    return d->mAtXEnd;
}

bool QMozOpenGLWebPage::atYBeginning() const
{
    return d->mAtYBeginning;
}

bool QMozOpenGLWebPage::atYEnd() const
{
    return d->mAtYEnd;
}

float QMozOpenGLWebPage::resolution() const
{
    return d->mContentResolution;
}

bool QMozOpenGLWebPage::isPainted() const
{
    return d->mIsPainted;
}

QColor QMozOpenGLWebPage::backgroundColor() const
{
    return d->mBackgroundColor;
}

bool QMozOpenGLWebPage::dragging() const
{
    return d->mDragging;
}

bool QMozOpenGLWebPage::moving() const
{
    return d->mMoving;
}

bool QMozOpenGLWebPage::pinching() const
{
    return d->mPinching;
}

QMozScrollDecorator *QMozOpenGLWebPage::verticalScrollDecorator() const
{
    return &d->mVerticalScrollDecorator;
}

QMozScrollDecorator *QMozOpenGLWebPage::horizontalScrollDecorator() const
{
    return &d->mHorizontalScrollDecorator;
}

bool QMozOpenGLWebPage::chromeGestureEnabled() const
{
    return d->mChromeGestureEnabled;
}

void QMozOpenGLWebPage::setChromeGestureEnabled(bool value)
{
    d->setChromeGestureEnabled(value);
}

qreal QMozOpenGLWebPage::chromeGestureThreshold() const
{
    return d->mChromeGestureThreshold;
}

void QMozOpenGLWebPage::setChromeGestureThreshold(qreal value)
{
    d->setChromeGestureThreshold(value);
}

bool QMozOpenGLWebPage::chrome() const
{
    return d->mChrome;
}

void QMozOpenGLWebPage::setChrome(bool value)
{
    d->setChrome(value);
}

qreal QMozOpenGLWebPage::contentWidth() const
{
    return d->mScrollableSize.width();
}

qreal QMozOpenGLWebPage::contentHeight() const
{
    return d->mScrollableSize.height();
}

int QMozOpenGLWebPage::dynamicToolbarHeight() const
{
    return d->mDynamicToolbarHeight;
}

void QMozOpenGLWebPage::setDynamicToolbarHeight(int height)
{
    d->setDynamicToolbarHeight(height);
}

QMargins QMozOpenGLWebPage::margins() const
{
    return d->mMargins;
}

void QMozOpenGLWebPage::setMargins(QMargins margins)
{
    d->setMargins(margins, true);
}

void QMozOpenGLWebPage::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_UNUSED(baseUrl);

    loadText(html, QStringLiteral("text/html"));
}

void QMozOpenGLWebPage::loadText(const QString &text, const QString &mimeType)
{
    d->load((QLatin1String("data:") + mimeType + QLatin1String(";charset=utf-8,")
             + QString::fromUtf8(QUrl::toPercentEncoding(text))), false);
}

void QMozOpenGLWebPage::goBack()
{
    d->goBack();
}

void QMozOpenGLWebPage::goForward()
{
    d->goForward();
}

void QMozOpenGLWebPage::stop()
{
    d->stop();
}

void QMozOpenGLWebPage::reload()
{
    d->reload();
}

void QMozOpenGLWebPage::load(const QString &url, const bool &fromExternal)
{
    d->load(url, fromExternal);
}

void QMozOpenGLWebPage::scrollTo(int x, int y)
{
    d->scrollTo(x, y);
}

void QMozOpenGLWebPage::scrollBy(int x, int y)
{
    d->scrollBy(x, y);
}

void QMozOpenGLWebPage::runJavaScript(const QString &script,
                                   const QJSValue &callback,
                                   const QJSValue &errorCallback)
{
    d->runJavaScript(script, callback, errorCallback);
}

// This should be a const method returning a pointer to a const object
// but unfortunately this conflicts with it being exposed as a Q_PROPERTY
QMozSecurity *QMozOpenGLWebPage::security()
{
    return &d->mSecurity;
}

void QMozOpenGLWebPage::sendAsyncMessage(const QString &name, const QVariant &value)
{
    d->sendAsyncMessage(name, value);
}

void QMozOpenGLWebPage::addMessageListener(const QString &name)
{
    d->addMessageListener(name.toStdString());
}

void QMozOpenGLWebPage::addMessageListeners(const std::vector<std::string> &messageNamesList)
{
    d->addMessageListeners(messageNamesList);
}

void QMozOpenGLWebPage::loadFrameScript(const QString &name)
{
    d->loadFrameScript(name);
}

void QMozOpenGLWebPage::newWindow(const QString &url)
{
#ifdef DEVELOPMENT_BUILD
    qCDebug(lcEmbedLiteExt) << "New Window:" << url.toUtf8().data();
#endif
}

quint32 QMozOpenGLWebPage::uniqueId() const
{
    return d->mView ? d->mView->GetUniqueID() : 0;
}

void QMozOpenGLWebPage::setParentId(unsigned parentId)
{
    d->setParentId(parentId);
}

void QMozOpenGLWebPage::setParentBrowsingContext(uintptr_t parentBrowsingContext)
{
    d->setParentBrowsingContext(parentBrowsingContext);
}

void QMozOpenGLWebPage::synthTouchBegin(const QVariant &touches)
{
    d->synthTouchBegin(touches);
}

void QMozOpenGLWebPage::synthTouchMove(const QVariant &touches)
{
    d->synthTouchMove(touches);
}

void QMozOpenGLWebPage::synthTouchEnd(const QVariant &touches)
{
    d->synthTouchEnd(touches);
}

void QMozOpenGLWebPage::suspendView()
{
    if (!d || !d->mViewInitialized) {
        return;
    }
    setActive(false);
    d->mView->SuspendTimeouts();
}

void QMozOpenGLWebPage::resumeView()
{
    if (!d || !d->mViewInitialized) {
        return;
    }
    setActive(true);

    // Setting view as active, will reset RefreshDriver()->SetThrottled at
    // PresShell::SetIsActive (nsPresShell). Thus, keep on throttling
    // if should keep on throttling.
    if (mThrottlePainting) {
        d->setThrottlePainting(true);
    }

    d->mView->ResumeTimeouts();
}

/*!
    \fn void QMozOpenGLWebPage::touchEvent(QTouchEvent *event)

    Touch events need to be in correctly mapped coordination
    system.
*/
void QMozOpenGLWebPage::touchEvent(QTouchEvent *event)
{
    d->touchEvent(event);
    event->accept();
}

void QMozOpenGLWebPage::timerEvent(QTimerEvent *event)
{
    d->timerEvent(event);
}

QString QMozOpenGLWebPage::httpUserAgent() const
{
    return d->httpUserAgent();
}

void QMozOpenGLWebPage::setHttpUserAgent(const QString &httpUserAgent)
{
    d->setHttpUserAgent(httpUserAgent);
}

bool QMozOpenGLWebPage::domContentLoaded() const
{
    return d->domContentLoaded();
}
