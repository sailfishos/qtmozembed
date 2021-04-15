/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qopenglwebpage.h"

#include <qglobal.h>
#include <qqmlinfo.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions_ES2>
#include <QWindow>
#include <QGuiApplication>
#include <QScreen>

#include "mozilla/embedlite/EmbedLiteApp.h"

#include "qmozview_p.h"
#include "mozilla/embedlite/EmbedLiteWindow.h"
#include "qmozcontext.h"
#include "qmozembedlog.h"
#include "qmozgrabresult.h"
#include "qmozscrolldecorator.h"
#include "qmozwindow.h"
#include "qmozwindow_p.h"

#define LOG_COMPONENT "QOpenGLWebPage"

using namespace mozilla;
using namespace mozilla::embedlite;


/*!
    \fn void QOpenGLWebPage::afterRendering()

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
    \fn void QOpenGLWebPage::QOpenGLWebPage(QObject *parent)

    In order to use this, embedlite.compositor.external_gl_context preference  needs to be set.
*/
QOpenGLWebPage::QOpenGLWebPage(QObject *parent)
    : QObject(parent)
    , d(new QMozViewPrivate(new IMozQView<QOpenGLWebPage>(*this), this))
    , mCompleted(false)
    , mSizeUpdateScheduled(false)
    , mThrottlePainting(false)
{
    d->mContext = QMozContext::instance();

    connect(this, &QOpenGLWebPage::viewInitialized, this, &QOpenGLWebPage::processViewInitialization);
    connect(this, &QOpenGLWebPage::loadProgressChanged, d, &QMozViewPrivate::updateLoaded);
    connect(this, &QOpenGLWebPage::loadingChanged, d, &QMozViewPrivate::updateLoaded);
}

QOpenGLWebPage::~QOpenGLWebPage()
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

void QOpenGLWebPage::createView()
{
    qCDebug(lcEmbedLiteExt) << "QOpenGLWebPage";
    if (!d->mView) {
        EmbedLiteWindow *win = d->mMozWindow->d->mWindow;
        d->mView = d->mContext->GetApp()->CreateView(win, d->mParentID, d->mPrivateMode, d->mDesktopMode);
        d->mView->SetListener(d);
        d->setDotsPerInch(QGuiApplication::primaryScreen()->physicalDotsPerInch());

        Q_EMIT uniqueIdChanged();
    }
}

void QOpenGLWebPage::processViewInitialization()
{
    // This is connected to view initialization. View must be initialized
    // over here.
    Q_ASSERT(d->mViewInitialized);

    mCompleted = true;
    setActive(true);
    Q_EMIT completedChanged();
}

void QOpenGLWebPage::onDrawOverlay(const QRect &rect)
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

int QOpenGLWebPage::parentId() const
{
    return d->mParentID;
}

bool QOpenGLWebPage::privateMode() const
{
    return d->mPrivateMode;
}

void QOpenGLWebPage::setPrivateMode(bool privateMode)
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

bool QOpenGLWebPage::enabled() const
{
    return d->mEnabled;
}

void QOpenGLWebPage::setEnabled(bool enabled)
{
    if (d->mEnabled != enabled) {
        d->mEnabled = enabled;
        Q_EMIT enabledChanged();
    }
}

bool QOpenGLWebPage::active() const
{
    return d->mActive;
}

void QOpenGLWebPage::setActive(bool active)
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

bool QOpenGLWebPage::loaded() const
{
    return d->mLoaded;
}

QMozWindow *QOpenGLWebPage::mozWindow() const
{
    return d->mMozWindow;
}

void QOpenGLWebPage::setMozWindow(QMozWindow *window)
{
    d->setMozWindow(window);

    if (window) {
        window->setSize(d->mSize.toSize());
    }

    connect(window, &QMozWindow::drawOverlay,
            this, &QOpenGLWebPage::onDrawOverlay, Qt::DirectConnection);
}

bool QOpenGLWebPage::desktopMode() const
{
    return d->mDesktopMode;
}

void QOpenGLWebPage::setDesktopMode(bool desktopMode)
{
    d->setDesktopMode(desktopMode);
}

bool QOpenGLWebPage::throttlePainting() const
{
    return mThrottlePainting;
}

void QOpenGLWebPage::setThrottlePainting(bool throttle)
{
    if (mThrottlePainting != throttle) {
        mThrottlePainting = throttle;
        d->setThrottlePainting(throttle);
        Q_EMIT throttlePaintingChanged();
    }
}

/*!
    \fn void QOpenGLWebPage::initialize()

    Call initialize to complete web page creation.
*/
void QOpenGLWebPage::initialize()
{
    Q_ASSERT(d->mMozWindow);
    if (!d->mContext->isInitialized()) {
        connect(d->mContext, &QMozContext::initialized, this, &QOpenGLWebPage::createView);
    } else {
        createView();
    }
}

bool QOpenGLWebPage::event(QEvent *event)
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

bool QOpenGLWebPage::completed() const
{
    return mCompleted;
}

void QOpenGLWebPage::update()
{
    if (!d->mViewInitialized) {
        return;
    }

    d->mView->ScheduleUpdate();
}

void QOpenGLWebPage::forceActiveFocus()
{
    if (!d->mViewInitialized) {
        return;
    }

    setActive(true);
    d->setIsFocused(true);
}

void QOpenGLWebPage::setInputMethodHints(Qt::InputMethodHints hints)
{
    d->mInputMethodHints = hints;
}

void QOpenGLWebPage::inputMethodEvent(QInputMethodEvent *event)
{
    d->inputMethodEvent(event);
}

void QOpenGLWebPage::keyPressEvent(QKeyEvent *event)
{
    d->keyPressEvent(event);
}

void QOpenGLWebPage::keyReleaseEvent(QKeyEvent *event)
{
    return d->keyReleaseEvent(event);
}

QVariant QOpenGLWebPage::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return d->inputMethodQuery(property);
}

void QOpenGLWebPage::focusInEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    d->setIsFocused(true);
}

void QOpenGLWebPage::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event);
    d->setIsFocused(false);
}

void QOpenGLWebPage::forceViewActiveFocus()
{
    forceActiveFocus();
}

QUrl QOpenGLWebPage::url() const
{
    return QUrl(d->mLocation);
}

void QOpenGLWebPage::setUrl(const QUrl &url)
{
    load(url.toString());
}

QString QOpenGLWebPage::title() const
{
    return d->mTitle;
}

int QOpenGLWebPage::loadProgress() const
{
    return d->mProgress;
}

bool QOpenGLWebPage::canGoBack() const
{
    return d->mCanGoBack;
}

bool QOpenGLWebPage::canGoForward() const
{
    return d->mCanGoForward;
}

bool QOpenGLWebPage::loading() const
{
    return d->mIsLoading;
}

QRectF QOpenGLWebPage::contentRect() const
{
    return d->mContentRect;
}

QSizeF QOpenGLWebPage::scrollableSize() const
{
    return d->mScrollableSize;
}

QPointF QOpenGLWebPage::scrollableOffset() const
{
    return d->mScrollableOffset;
}

bool QOpenGLWebPage::atXBeginning() const
{
    return d->mAtXBeginning;
}

bool QOpenGLWebPage::atXEnd() const
{
    return d->mAtXEnd;
}

bool QOpenGLWebPage::atYBeginning() const
{
    return d->mAtYBeginning;
}

bool QOpenGLWebPage::atYEnd() const
{
    return d->mAtYEnd;
}

float QOpenGLWebPage::resolution() const
{
    return d->mContentResolution;
}

bool QOpenGLWebPage::isPainted() const
{
    return d->mIsPainted;
}

QColor QOpenGLWebPage::bgcolor() const
{
    return d->mBgColor;
}

bool QOpenGLWebPage::getUseQmlMouse()
{
    return false;
}

void QOpenGLWebPage::setUseQmlMouse(bool value)
{
    Q_UNUSED(value);
}

bool QOpenGLWebPage::dragging() const
{
    return d->mDragging;
}

bool QOpenGLWebPage::moving() const
{
    return d->mMoving;
}

bool QOpenGLWebPage::pinching() const
{
    return d->mPinching;
}

QMozScrollDecorator *QOpenGLWebPage::verticalScrollDecorator() const
{
    return &d->mVerticalScrollDecorator;
}

QMozScrollDecorator *QOpenGLWebPage::horizontalScrollDecorator() const
{
    return &d->mHorizontalScrollDecorator;
}

bool QOpenGLWebPage::chromeGestureEnabled() const
{
    return d->mChromeGestureEnabled;
}

void QOpenGLWebPage::setChromeGestureEnabled(bool value)
{
    d->setChromeGestureEnabled(value);
}

qreal QOpenGLWebPage::chromeGestureThreshold() const
{
    return d->mChromeGestureThreshold;
}

void QOpenGLWebPage::setChromeGestureThreshold(qreal value)
{
    d->setChromeGestureThreshold(value);
}

bool QOpenGLWebPage::chrome() const
{
    return d->mChrome;
}

void QOpenGLWebPage::setChrome(bool value)
{
    d->setChrome(value);
}

qreal QOpenGLWebPage::contentWidth() const
{
    return d->mScrollableSize.width();
}

qreal QOpenGLWebPage::contentHeight() const
{
    return d->mScrollableSize.height();
}

QMargins QOpenGLWebPage::margins() const
{
    return d->mMargins;
}

void QOpenGLWebPage::setMargins(QMargins margins)
{
    d->setMargins(margins, true);
}

void QOpenGLWebPage::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_UNUSED(baseUrl);

    loadText(html, QStringLiteral("text/html"));
}

void QOpenGLWebPage::loadText(const QString &text, const QString &mimeType)
{
    d->load((QLatin1String("data:") + mimeType + QLatin1String(";charset=utf-8,") + QString::fromUtf8(QUrl::toPercentEncoding(text))));
}


void QOpenGLWebPage::goBack()
{
    if (!d->mViewInitialized)
        return;
    d->mView->GoBack();
}

void QOpenGLWebPage::goForward()
{
    if (!d->mViewInitialized)
        return;
    d->mView->GoForward();
}

void QOpenGLWebPage::stop()
{
    if (!d->mViewInitialized)
        return;
    d->mView->StopLoad();
}

void QOpenGLWebPage::reload()
{
    if (!d->mViewInitialized)
        return;
    d->resetPainted();
    d->mView->Reload(false);
}

void QOpenGLWebPage::load(const QString &url)
{
    d->load(url);
}

void QOpenGLWebPage::scrollTo(int x, int y)
{
    d->scrollTo(x, y);
}

void QOpenGLWebPage::scrollBy(int x, int y)
{
    d->scrollBy(x, y);
}

// This should be a const method returning a pointer to a const object
// but unfortunately this conflicts with it being exposed as a Q_PROPERTY
QMozSecurity *QOpenGLWebPage::security()
{
    return &d->mSecurity;
}

void QOpenGLWebPage::sendAsyncMessage(const QString &name, const QVariant &value)
{
    d->sendAsyncMessage(name, value);
}

void QOpenGLWebPage::addMessageListener(const QString &name)
{
    d->addMessageListener(name.toStdString());
}

void QOpenGLWebPage::addMessageListeners(const std::vector<std::string> &messageNamesList)
{
    d->addMessageListeners(messageNamesList);
}

void QOpenGLWebPage::loadFrameScript(const QString &name)
{
    d->loadFrameScript(name);
}

void QOpenGLWebPage::newWindow(const QString &url)
{
#ifdef DEVELOPMENT_BUILD
    qCDebug(lcEmbedLiteExt) << "New Window:" << url.toUtf8().data();
#endif
}

quint32 QOpenGLWebPage::uniqueId() const
{
    return d->mView ? d->mView->GetUniqueID() : 0;
}

void QOpenGLWebPage::setParentId(unsigned parentId)
{
    d->setParentId(parentId);
}

void QOpenGLWebPage::synthTouchBegin(const QVariant &touches)
{
    d->synthTouchBegin(touches);
}

void QOpenGLWebPage::synthTouchMove(const QVariant &touches)
{
    d->synthTouchMove(touches);
}

void QOpenGLWebPage::synthTouchEnd(const QVariant &touches)
{
    d->synthTouchEnd(touches);
}

void QOpenGLWebPage::suspendView()
{
    if (!d || !d->mViewInitialized) {
        return;
    }
    setActive(false);
    d->mView->SuspendTimeouts();
}

void QOpenGLWebPage::resumeView()
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

void QOpenGLWebPage::recvMouseMove(int posX, int posY)
{
    d->recvMouseMove(posX, posY);
}

void QOpenGLWebPage::recvMousePress(int posX, int posY)
{
    d->recvMousePress(posX, posY);
}

void QOpenGLWebPage::recvMouseRelease(int posX, int posY)
{
    d->recvMouseRelease(posX, posY);
}

/*!
    \fn void QOpenGLWebPage::touchEvent(QTouchEvent *event)

    Touch events need to be in correctly mapped coordination
    system.
*/
void QOpenGLWebPage::touchEvent(QTouchEvent *event)
{
    d->touchEvent(event);
    event->accept();
}

void QOpenGLWebPage::timerEvent(QTimerEvent *event)
{
    d->timerEvent(event);
}
