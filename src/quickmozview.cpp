/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "quickmozview.h"

#include "mozilla-config.h"
#include "qmozcontext.h"
#include "qmozembedlog.h"
#include "mozilla/embedlite/EmbedLiteView.h"
#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/TimeStamp.h"

#include <QThread>
#include <QMutexLocker>
#include <QtQuick/qquickwindow.h>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QtOpenGLExtensions>
#include <QQmlInfo>

#include "qmozview_p.h"
#include "qmozextmaterialnode.h"
#include "qmozscrolldecorator.h"
#include "qmozexttexture.h"
#include "qmozwindow.h"
#include "qmozwindow_p.h"

using namespace mozilla;
using namespace mozilla::embedlite;


namespace {

class ObjectCleanup : public QRunnable
{
public:
    ObjectCleanup(QObject *object)
        : m_object(object)
    {
    }

    void run()
    {
        delete m_object;
    }

private:
    QObject * const m_object;
};

}

QSizeF webContentWindowSize(Qt::ScreenOrientation orientation, const QSizeF &size)
{
    // Set size for EmbedLiteWindow in "portrait"
    QSizeF s = size;
    if (orientation == Qt::LandscapeOrientation || orientation == Qt::InvertedLandscapeOrientation) {
        s.transpose();
    }
    return s;
}

QuickMozView::QuickMozView(QQuickItem *parent)
    : QQuickItem(parent)
    , d(new QMozViewPrivate(new IMozQView<QuickMozView>(*this), this))
    , mTexture(nullptr)
    , mParentID(0)
    , mOrientation(Qt::PrimaryOrientation)
    , mExplicitViewportWidth(false)
    , mExplicitViewportHeight(false)
    , mExplicitOrientation(false)
    , mPrivateMode(false)
    , mUseQmlMouse(false)
    , mComposited(false)
    , mActive(false)
    , mLoaded(false)
    , mFollowItemGeometry(true)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setFlag(ItemIsFocusScope, true);
    setFlag(ItemAcceptsDrops, true);
    setFlag(ItemAcceptsInputMethod, true);

    d->mContext = QMozContext::instance();
    connect(this, &QuickMozView::setIsActive, this, &QuickMozView::SetIsActive);
    connect(this, &QuickMozView::viewInitialized, this, &QuickMozView::processViewInitialization);
    connect(this, &QuickMozView::enabledChanged, this, &QuickMozView::updateEnabled);
    connect(this, &QuickMozView::loadProgressChanged, this, &QuickMozView::updateLoaded);
    connect(this, &QuickMozView::loadingChanged, this, &QuickMozView::updateLoaded);
    connect(this, &QuickMozView::scrollableOffsetChanged, this, &QuickMozView::updateMargins);
    connect(this, &QuickMozView::firstPaint, this, &QQuickItem::update);
    updateEnabled();
}

QuickMozView::~QuickMozView()
{
    releaseResources();

    if (d->mView) {
        d->mView->SetListener(NULL);
        d->mContext->GetApp()->DestroyView(d->mView);
    }
    delete d;
    d = 0;
}

void
QuickMozView::SetIsActive(bool aIsActive)
{
    if (QThread::currentThread() == thread() && d->mView) {
        d->mView->SetIsActive(aIsActive);
    } else {
        Q_EMIT setIsActive(aIsActive);
    }
}

void QuickMozView::updateLoaded()
{
    bool loaded = loadProgress() == 100 && !loading();
    if (mLoaded != loaded) {
        mLoaded = loaded;
        Q_EMIT loadedChanged();
    }
}

void
QuickMozView::contextInitialized()
{
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    createView();
}

void QuickMozView::processViewInitialization()
{
    // This is connected to view initialization. View must be initialized
    // over here.
    Q_ASSERT(d->mViewInitialized);
    SetIsActive(mActive);
}

void QuickMozView::updateEnabled()
{
    d->mEnabled = QQuickItem::isEnabled();
}

void QuickMozView::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == ItemSceneChange) {
        if (data.window) {
            connect(data.window, &QQuickWindow::contentOrientationChanged, this, &QuickMozView::updateOrientation);

            updateOrientation(data.window->contentOrientation());
        }
    }
    QQuickItem::itemChange(change, data);
}

void QuickMozView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    updateContentSize(QSizeF(
                mExplicitViewportWidth ? d->mSize.width() : newGeometry.width(),
                mExplicitViewportHeight ? d->mSize.height() : newGeometry.height()));

    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void QuickMozView::createView()
{
    if (d->mSize.isEmpty()) {
        d->mSize = window()->size();
    }

    QMozWindow *mozWindow = d->mContext->registeredWindow();
    if (!mozWindow) {
        mozWindow = new QMozWindow(webContentWindowSize(mOrientation, d->mSize).toSize());
        d->mContext->registerWindow(mozWindow);
    } else if (d->mDirtyState & QMozViewPrivate::DirtySize && mActive) {
        mozWindow->setSize(webContentWindowSize(mOrientation, d->mSize).toSize());
    }

    if (mActive) {
        mozWindow->setContentOrientation(mOrientation);
    }

    d->setMozWindow(mozWindow);
    d->mView = d->mContext->GetApp()->CreateView(d->mMozWindow->d->mWindow, mParentID, mPrivateMode);
    d->mView->SetListener(d);
    d->setDotsPerInch(QGuiApplication::primaryScreen()->physicalDotsPerInch());
    connect(d->mMozWindow.data(), &QMozWindow::compositingFinished,
            this, &QuickMozView::compositingFinished);
}

QSGNode *
QuickMozView::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    // If the dimensions are entirely invalid return no node.
    if (width() <= 0 || height() <= 0) {
        delete oldNode;

        delete mTexture;
        mTexture = nullptr;

        return nullptr;
    }

    const bool invalidTexture = !mComposited
            || !d->mIsPainted
            || !d->mViewInitialized
            || !d->mHasCompositor
            || !d->mContext->registeredWindow()
            || !d->mMozWindow;

    if (mTexture && invalidTexture) {
        delete oldNode;
        oldNode = nullptr;

        delete mTexture;
        mTexture = nullptr;
    }

    QRectF boundingRect(d->renderingOffset(), d->mSize);

    if (!mTexture && invalidTexture) {
        QSGSimpleRectNode *node = static_cast<QSGSimpleRectNode *>(oldNode);
        if (!node) {
            node = new QSGSimpleRectNode;
        }
        node->setColor(d->mBgColor);
        node->setRect(boundingRect);

        return node;
    }

    if (!mTexture) {
        delete oldNode;
        oldNode = nullptr;
    }

    MozMaterialNode *node = static_cast<MozMaterialNode *>(oldNode);

    if (!node) {
#if defined(QT_OPENGL_ES_2)
        QMozExtTexture * const texture = new QMozExtTexture;
        mTexture = texture;

        connect(texture, &QMozExtTexture::getPlatformImage, d->mMozWindow, &QMozWindow::getPlatformImage, Qt::DirectConnection);

        node = new MozExtMaterialNode;
#else
#warning "Implement me for non ES2 platform"
//        node = new MozRgbMaterialNode;
        return nullptr;
#endif

        node->setTexture(mTexture);
    }

    node->setRect(boundingRect);
    node->setOrientation(mOrientation);
    node->markDirty(QSGNode::DirtyMaterial);

    return node;
}

void QuickMozView::releaseResources()
{
#if defined(QT_OPENGL_ES_2)
    if (QMozExtTexture * const texture = d->mMozWindow ? qobject_cast<QMozExtTexture *>(mTexture) : nullptr) {
        disconnect(texture, &QMozExtTexture::getPlatformImage, d->mMozWindow, &QMozWindow::getPlatformImage);
    }
#endif

    if (QQuickWindow * const window = mTexture ? QQuickItem::window() : nullptr) {
        window->scheduleRenderJob(new ObjectCleanup(mTexture), QQuickWindow::AfterSynchronizingStage);
        mTexture = nullptr;
    }
}

int QuickMozView::parentId() const
{
    return mParentID;
}

bool QuickMozView::privateMode() const
{
    return mPrivateMode;
}

bool QuickMozView::active() const
{
    return mActive;
}

void QuickMozView::setActive(bool active)
{
    if (d->mViewInitialized) {
        if (mActive != active) {
            mActive = active;
            // Process pending paint request before final suspend (unblock possible content Compositor waiters Bug 1020350)
            SetIsActive(active);
            if (active) {
                resumeRendering();
                polish();
            } else {
                mComposited = false;
                update();
            }
            Q_EMIT activeChanged();
        }
    } else {
        // Will be processed once view is initialized.
        mActive = active;
    }
}

bool QuickMozView::loaded() const
{
    return mLoaded;
}

/*!
 * \fn QuickMozView::updateContentSize(const QSize &size)
 * Updates web content size to given \a size. Web content size can be
 * also smaller or greater than QuickMozView size.
 *
 * NB: We are omitting square size as that introduces glitches when
 * rotating item. Downside is that by doing this we break intentional
 * resizing to square.
 */
void QuickMozView::updateContentSize(const QSizeF &size)
{
    const QSizeF originalSize = d->mSize;

    polish();

    d->setSize(size);

    if (d->mSize.width() != originalSize.width()) {
        Q_EMIT viewportWidthChanged();
    }
    if (d->mSize.height() != originalSize.height()) {
        Q_EMIT viewportHeightChanged();
    }
}

void QuickMozView::compositingFinished()
{
    if (mActive) {
        mComposited = true;
        update();
    }
}

void QuickMozView::updateMargins()
{
    if (d->mViewInitialized) {
        QPointF offset = scrollableOffset();
        QMargins m = margins();
        if (offset.y() > m.top()) {
            m.setTop(0);
        } else {
            m.setTop(d->mTopMargin);
        }

        if ((offset.y() + height() + d->mBottomMargin) < contentHeight()) {
            m.setBottom(0);
        } else {
            m.setBottom(d->mBottomMargin);
        }

        d->SetMargins(m, false);
    }
}

void QuickMozView::mouseMoveEvent(QMouseEvent *e)
{
    if (!mUseQmlMouse) {
        const bool accepted = e->isAccepted();
        recvMouseMove(e->pos().x(), e->pos().y());
        e->setAccepted(accepted);
    } else {
        QQuickItem::mouseMoveEvent(e);
    }
}

void QuickMozView::mousePressEvent(QMouseEvent *e)
{
    if (!mUseQmlMouse) {
        const bool accepted = e->isAccepted();
        recvMousePress(e->pos().x(), e->pos().y());
        e->setAccepted(accepted);
    } else {
        QQuickItem::mousePressEvent(e);
    }
}

void QuickMozView::mouseReleaseEvent(QMouseEvent *e)
{
    if (!mUseQmlMouse) {
        const bool accepted = e->isAccepted();
        recvMouseRelease(e->pos().x(), e->pos().y());
        e->setAccepted(accepted);
    } else {
        QQuickItem::mouseReleaseEvent(e);
    }
}

void QuickMozView::setInputMethodHints(Qt::InputMethodHints hints)
{
    d->mInputMethodHints = hints;
}

void QuickMozView::inputMethodEvent(QInputMethodEvent *event)
{
    d->inputMethodEvent(event);
}

void QuickMozView::keyPressEvent(QKeyEvent *event)
{
    d->keyPressEvent(event);
}

void QuickMozView::keyReleaseEvent(QKeyEvent *event)
{
    d->keyReleaseEvent(event);
}

QVariant QuickMozView::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return d->inputMethodQuery(property);
}

void QuickMozView::focusInEvent(QFocusEvent *event)
{
    d->SetIsFocused(true);
    QQuickItem::focusInEvent(event);
}

void QuickMozView::focusOutEvent(QFocusEvent *event)
{
    d->SetIsFocused(false);
    QQuickItem::focusOutEvent(event);
}

void QuickMozView::forceViewActiveFocus()
{
    forceActiveFocus();
    if (d->mViewInitialized) {
        setActive(true);
        d->mView->SetIsFocused(true);
    }
}

QUrl QuickMozView::url() const
{
    return QUrl(d->mLocation);
}

void QuickMozView::setUrl(const QUrl &url)
{
    load(url.toString());
}

QString QuickMozView::title() const
{
    return d->mTitle;
}

int QuickMozView::loadProgress() const
{
    return d->mProgress;
}

bool QuickMozView::canGoBack() const
{
    return d->mCanGoBack;
}

bool QuickMozView::canGoForward() const
{
    return d->mCanGoForward;
}

bool QuickMozView::loading() const
{
    return d->mIsLoading;
}

QRectF QuickMozView::contentRect() const
{
    return d->mContentRect;
}

QSizeF QuickMozView::scrollableSize() const
{
    return d->mScrollableSize;
}

QPointF QuickMozView::scrollableOffset() const
{
    return d->mScrollableOffset;
}

bool QuickMozView::atXBeginning() const
{
    return d->mAtXBeginning;
}

bool QuickMozView::atXEnd() const
{
    return d->mAtXEnd;
}

bool QuickMozView::atYBeginning() const
{
    return d->mAtYBeginning;
}

bool QuickMozView::atYEnd() const
{
    return d->mAtYEnd;
}

float QuickMozView::resolution() const
{
    return d->mContentResolution;
}

bool QuickMozView::isPainted() const
{
    return d->mIsPainted;
}

QColor QuickMozView::bgcolor() const
{
    return d->mBgColor;
}

bool QuickMozView::getUseQmlMouse()
{
    return mUseQmlMouse;
}

void QuickMozView::setUseQmlMouse(bool value)
{
    mUseQmlMouse = value;
}

bool QuickMozView::dragging() const
{
    return d->mDragging;
}

bool QuickMozView::moving() const
{
    return d->mMoving;
}

bool QuickMozView::pinching() const
{
    return d->mPinching;
}

QMozScrollDecorator *QuickMozView::verticalScrollDecorator() const
{
    return &d->mVerticalScrollDecorator;
}

QMozScrollDecorator *QuickMozView::horizontalScrollDecorator() const
{
    return &d->mHorizontalScrollDecorator;
}

bool QuickMozView::chromeGestureEnabled() const
{
    return d->mChromeGestureEnabled;
}

void QuickMozView::setChromeGestureEnabled(bool value)
{
    if (value != d->mChromeGestureEnabled) {
        d->mChromeGestureEnabled = value;
        Q_EMIT chromeGestureEnabledChanged();
    }
}

qreal QuickMozView::chromeGestureThreshold() const
{
    return d->mChromeGestureThreshold;
}

void QuickMozView::setChromeGestureThreshold(qreal value)
{
    if (value != d->mChromeGestureThreshold) {
        d->mChromeGestureThreshold = value;
        Q_EMIT chromeGestureThresholdChanged();
    }
}

bool QuickMozView::chrome() const
{
    return d->mChrome;
}

void QuickMozView::setChrome(bool value)
{
    if (value != d->mChrome) {
        d->mChrome = value;
        Q_EMIT chromeChanged();
    }
}

qreal QuickMozView::contentWidth() const
{
    return d->mScrollableSize.width();
}

qreal QuickMozView::contentHeight() const
{
    return d->mScrollableSize.height();
}

QMargins QuickMozView::margins() const
{
    return d->mMargins;
}

Qt::ScreenOrientation QuickMozView::orientation() const
{
    return mOrientation;
}

void QuickMozView::setOrientation(Qt::ScreenOrientation orientation)
{
    mExplicitOrientation = true;
    if (mOrientation != orientation && orientation != Qt::PrimaryOrientation) {
        polish();

        mOrientation = orientation;

        Q_EMIT orientationChanged();
    }
}

void QuickMozView::resetOrientation()
{
    mExplicitOrientation = false;
    if (QQuickWindow *window = this->window()) {
        updateOrientation(window->contentOrientation());
    }
}

void QuickMozView::updateOrientation(Qt::ScreenOrientation orientation)
{
    if (!mExplicitOrientation) {
        polish();

        if (mOrientation != orientation) {
            mOrientation = orientation;

            Q_EMIT orientationChanged();
        }
    }
}

qreal QuickMozView::viewportWidth() const
{
    return d->mSize.width();
}

void QuickMozView::setViewportWidth(qreal width)
{
    mExplicitViewportWidth = true;
    updateContentSize(QSize(width, d->mSize.height()));
}

void QuickMozView::resetViewportWidth()
{
    mExplicitViewportWidth = false;
    updateContentSize(QSize(width(), d->mSize.height()));
}

qreal QuickMozView::viewportHeight() const
{
    return d->mSize.height();
}

void QuickMozView::setViewportHeight(qreal height)
{
    mExplicitViewportHeight = true;
    updateContentSize(QSize(d->mSize.width(), height));
}

void QuickMozView::resetViewportHeight()
{
    mExplicitViewportHeight = false;
    updateContentSize(QSize(d->mSize.width(), height()));
}

void QuickMozView::setMargins(QMargins margins)
{
    d->SetMargins(margins, true);
}

void QuickMozView::loadHtml(const QString &html, const QUrl &baseUrl)
{
    Q_UNUSED(baseUrl)

    loadText(html, QStringLiteral("text/html"));
}

void QuickMozView::loadText(const QString &text, const QString &mimeType)
{
    d->load((QLatin1String("data:") + mimeType + QLatin1String(";charset=utf-8,") + QString::fromUtf8(QUrl::toPercentEncoding(text))));
}

void QuickMozView::goBack()
{
    if (!d->mViewInitialized)
        return;

    d->ResetPainted();
    d->mView->GoBack();
}

void QuickMozView::goForward()
{
    if (!d->mViewInitialized)
        return;

    d->ResetPainted();
    d->mView->GoForward();
}

void QuickMozView::stop()
{
    if (!d->mViewInitialized)
        return;
    d->mView->StopLoad();
}

void QuickMozView::reload()
{
    if (!d->mViewInitialized)
        return;

    d->ResetPainted();
    d->mView->Reload(false);
}

void QuickMozView::load(const QString &url)
{
    d->load(url);
}

void QuickMozView::scrollTo(int x, int y)
{
    d->scrollTo(x, y);
}

void QuickMozView::scrollBy(int x, int y)
{
    d->scrollBy(x, y);
}

// This should be a const method returning a pointer to a const object
// but unfortunately this conflicts with it being exposed as a Q_PROPERTY
QMozSecurity *QuickMozView::security()
{
    return &d->mSecurity;
}

void QuickMozView::sendAsyncMessage(const QString &name, const QVariant &variant)
{
    d->sendAsyncMessage(name, variant);
}

void QuickMozView::addMessageListener(const QString &name)
{
    d->addMessageListener(name.toStdString());
}

void QuickMozView::addMessageListeners(const std::vector<std::string> &messageNamesList)
{
    d->addMessageListeners(messageNamesList);
}

void QuickMozView::loadFrameScript(const QString &name)
{
    d->loadFrameScript(name);
}

void QuickMozView::newWindow(const QString &url)
{
#ifdef DEVELOPMENT_BUILD
    qCDebug(lcEmbedLiteExt) << "New Window:" << url.toUtf8().data();
#endif
}

quint32 QuickMozView::uniqueID() const
{
    return d->mView ? d->mView->GetUniqueID() : 0;
}

void QuickMozView::setParentID(unsigned aParentID)
{
    if (aParentID != mParentID) {
        mParentID = aParentID;
        Q_EMIT parentIdChanged();
    }
}

void QuickMozView::setPrivateMode(bool aPrivateMode)
{
    if (isComponentComplete()) {
        // View is created directly in componentComplete() if mozcontext ready
        qmlInfo(this) << "privateMode cannot be changed after view is created";
        return;
    }

    if (aPrivateMode != mPrivateMode) {
        mPrivateMode = aPrivateMode;
        Q_EMIT privateModeChanged();
    }
}

void QuickMozView::synthTouchBegin(const QVariant &touches)
{
    d->synthTouchBegin(touches);
}

void QuickMozView::synthTouchMove(const QVariant &touches)
{
    d->synthTouchMove(touches);
}

void QuickMozView::synthTouchEnd(const QVariant &touches)
{
    d->synthTouchEnd(touches);
}

void QuickMozView::suspendView()
{
    if (!d->mViewInitialized) {
        return;
    }
    setActive(false);
    d->mView->SuspendTimeouts();
    d->mMozWindow->suspendRendering();
}

void QuickMozView::resumeView()
{
    if (!d->mViewInitialized) {
        return;
    }
    setActive(true);
    d->mView->ResumeTimeouts();
}

void QuickMozView::recvMouseMove(int posX, int posY)
{
    d->recvMouseMove(posX, posY);
}

void QuickMozView::recvMousePress(int posX, int posY)
{
    d->recvMousePress(posX, posY);
}

void QuickMozView::recvMouseRelease(int posX, int posY)
{
    d->recvMouseRelease(posX, posY);
}

void QuickMozView::touchEvent(QTouchEvent *event)
{
    if (!mUseQmlMouse || event->touchPoints().count() > 1) {
        d->touchEvent(event);
    } else {
        QQuickItem::touchEvent(event);
    }
}

void QuickMozView::timerEvent(QTimerEvent *event)
{
    d->timerEvent(event);
    if (!event->isAccepted()) {
        QQuickItem::timerEvent(event);
    }
}

void QuickMozView::componentComplete()
{
    QQuickItem::componentComplete();
    // The first created view gets always parentId of 0
    if (!d->mContext->isInitialized()) {
        connect(d->mContext, &QMozContext::onInitialized, this, &QuickMozView::contextInitialized);
    } else {
        createView();
    }
}

void QuickMozView::resumeRendering()
{
    d->mMozWindow->resumeRendering();
}

void QuickMozView::updatePolish()
{
    if (d->mMozWindow && mActive) {
        d->mMozWindow->setContentOrientation(mOrientation);
        d->mMozWindow->setSize(webContentWindowSize(mOrientation, d->mSize).toSize());
    }
}
