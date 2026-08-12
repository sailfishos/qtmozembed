/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "QMozContext"

#include <QVariant>
#include <QThread>
#include <QTimer>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtQml/qqml.h>
#include <QJSValue>

#include <dlfcn.h>
#include <link.h>

#include "qmozembedlog.h"
#include "qmozcontext.h"
#include "qmozcontext_p.h"
#include "qmozenginesettings.h"
#include "qmozviewcreator.h"
#include "geckoworker.h"
#include "qmozwindow.h"
#include "runtime/qmozchromewindowregistry_p.h"
#include "runtime/qmozruntime_p.h"

#include "mozilla/embedlite/EmbedLiteView.h"

Q_LOGGING_CATEGORY(lcEmbedLiteExt, "org.sailfishos.embedliteext", QtWarningMsg)

using namespace mozilla::embedlite;

Q_GLOBAL_STATIC(QMozContext, mozContextInstance)
Q_GLOBAL_STATIC(QMozContextPrivate, mozContextPrivateInstance)

// Pre-load the eglplatform_wayland.so dynamic library to avoid it being
// closed too early later, which will cause a crash. This is due to a
// lack of reference counting in ws_init and ws_Terminate here:
// https://github.com/libhybris/libhybris/blob/master/hybris/egl/ws.c#L99
// This should be reverted once the following change to libhybris has
// been rolled out:
// https://github.com/libhybris/libhybris/pull/563
static const bool platformEglWorkaround = !getenv("DISABLE_PLAT_EGL_FIX");
static void *platformEglHandle = nullptr;

static void platform_egl_workaround_open() {
  if (platformEglHandle || !platformEglWorkaround) {
    // Only load once
    return;
  }

  // Find the location of the library
  QString foundPath;
  dl_iterate_phdr(
      [](dl_phdr_info* info, size_t, void* data) {
        QString& foundPath = *reinterpret_cast<QString*>(data);
        QString thisPath(info->dlpi_name);
        if (thisPath.endsWith("/eglplatform_wayland.so")) {
          foundPath = thisPath;
          return 1;
        }
        return 0;
      },
      &foundPath);

  // Open the library
  qCDebug(lcEmbedLiteExt) << "Pre-loading eglplatform_wayland.so at from" << foundPath;
  if (!foundPath.isEmpty()) {
    platformEglHandle = dlopen(foundPath.toLatin1(), RTLD_LAZY);
    if (!platformEglHandle) {
      qCWarning(lcEmbedLiteExt) << "Error pre-loading eglplatform_wayland.so";
    }
  }
}

static void platform_egl_workaround_close() {
  if (platformEglHandle && platformEglWorkaround) {
    dlclose(platformEglHandle);
    platformEglHandle = nullptr;
  }
}

QMozContextPrivate *QMozContextPrivate::instance()
{
    return mozContextPrivateInstance();
}

QMozContextPrivate::QMozContextPrivate(QObject *parent)
    : QObject(parent)
    , mRuntime(nullptr)
    , mInitialized(false)
    , mThread(new QThread())
    , mEmbedStarted(false)
    , mQtPump(nullptr)
    , mAsyncContext(!getenv("DISABLE_ASYNC"))
    , mViewCreator(nullptr)
    , mMozWindow(nullptr)
{
    qCDebug(lcEmbedLiteExt) << "Create new Context:" << (void *)this
                            << ", parent:" << (void *)parent << getenv("GRE_HOME");

    platform_egl_workaround_open();

    setenv("BUILD_GRE_HOME", BUILD_GRE_HOME, 1);
    // See JB#11625: JSON message are locale aware avoid breaking them
    // This is moved from the sailfish-components-webview.
    setenv("LC_NUMERIC", "C", 1);
    setlocale(LC_NUMERIC, "C");

    // GRE_HOME must be set before QMozContext is initialized. With invoker PWD is empty.
    QByteArray binaryPath = QCoreApplication::applicationDirPath().toLocal8Bit();
    setenv("GRE_HOME", binaryPath.constData(), 1);

    mRuntime = new QMozRuntime(this, mAsyncContext, this);
}

QMozContextPrivate::~QMozContextPrivate()
{
    destroyWindow();

    platform_egl_workaround_close();
}

bool QMozContextPrivate::ExecuteChildThread()
{
    return false;
}

// Native thread must be stopped here
bool QMozContextPrivate::StopChildThread()
{
    if (mThread && !mThread->isFinished()) {
        qCDebug(lcEmbedLiteExt) << "Stop Native thread:" << (void *)mThread;
        mThread->exit(0);
        mThread->wait();
        return true;
    }
    return false;
}

// App Initialized and ready to API call
void QMozContextPrivate::Initialized()
{
    mInitialized = true;
    mRuntime->embedLiteApp()->LoadGlobalStyleSheet(
            "chrome://global/content/embedScrollStyles.css", true);

    std::vector<std::string> observersList;
    observersList.reserve(mObservers.size());
    for (const std::pair<const std::string, uint> &observer : mObservers) {
        if (observer.second > 0) {
            observersList.push_back(observer.first);
        }
    }
    if (observersList.size() > 0) {
        mRuntime->embedLiteApp()->AddObservers(observersList);
    }

    Q_EMIT initialized();
}

// App Destroyed, and ready to delete and program exit
void QMozContextPrivate::Destroyed()
{
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    mRuntime->detachListener();

    if (mThread && !mThread->isFinished()) {
        mThread->exit(0);
        mThread->wait();
        mThread = nullptr;
    }

    mRuntime->backendDestroyed();
    Q_EMIT contextDestroyed();
}

void QMozContextPrivate::OnObserve(const char *aTopic, const char16_t *aData)
{
    //qCDebug(lcEmbedLiteExt) << "aTopic:" << aTopic << ", data:" << NS_ConvertUTF16toUTF8(aData).get();
    QString data((QChar *)aData);
    if (!data.startsWith('{') && !data.startsWith('[') && !data.startsWith('"')) {
        QVariant vdata = QVariant::fromValue(data);
        Q_EMIT recvObserve(aTopic, vdata);
        return;
    }
    bool ok = true;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &error);
    ok = error.error == QJsonParseError::NoError;
    QVariant vdata = doc.toVariant();
    if (ok) {
        //qCDebug(lcEmbedLiteExt) << "mesg:" << aTopic << ", data:" << data.toUtf8().data();
        Q_EMIT recvObserve(aTopic, vdata);
    } else {
        qCDebug(lcEmbedLiteExt) << "JSON parse error:" << error.errorString().toUtf8().data();
#ifdef DEVELOPMENT_BUILD
        qCDebug(lcEmbedLiteExt) << "parse: s:'" << data.toUtf8().data() << "', errLine:" << error.offset;
#endif
    }
}

void QMozContextPrivate::destroyWindow()
{
    if (!mMozWindow) return;

    if (mMozWindow->isReserved()) {
        connect(mMozWindow.data(), &QMozWindow::released,
                mMozWindow.data(), &QObject::deleteLater);
        mMozWindow->release();
    } else {
        delete mMozWindow;
    }
    mMozWindow = nullptr;
}

void QMozContextPrivate::LastViewDestroyed()
{
    Q_EMIT lastViewDestroyed();
}

void QMozContextPrivate::LastWindowDestroyed()
{
    Q_EMIT lastWindowDestroyed();
}

bool QMozContextPrivate::IsInitialized()
{
    return mRuntime->hasApp() && mInitialized;
}

uint32_t QMozContextPrivate::CreateNewWindowRequested(const uint32_t &chromeFlags, const bool &hidden,
                                                      EmbedLiteView *aParentView, const uintptr_t &parentBrowsingContext)
{
    Q_UNUSED(chromeFlags)

    uint32_t parentId = aParentView ? aParentView->GetUniqueID() : 0;
    qCDebug(lcEmbedLiteExt) << "QtMozEmbedContext new Window requested: parent:" << (void *)aParentView << parentId;
    uint32_t viewId = QMozContext::instance()->createView(parentId, parentBrowsingContext, hidden);
    return viewId;
}

EmbedLiteMessagePump *QMozContextPrivate::EmbedLoop()
{
    return mRuntime->embedLoop();
}

QMozContext *QMozContext::instance()
{
    return mozContextInstance();
}

QMozContext::QMozContext(QObject *parent)
    : QObject(parent)
    , d(QMozContextPrivate::instance())
{
    connect(d, &QMozContextPrivate::initialized, this, &QMozContext::initialized);
    connect(d, &QMozContextPrivate::contextDestroyed, this, &QMozContext::contextDestroyed);
    connect(d, &QMozContextPrivate::lastViewDestroyed, this, &QMozContext::lastViewDestroyed);
    connect(d, &QMozContextPrivate::lastWindowDestroyed, this, &QMozContext::lastWindowDestroyed);
    connect(d, &QMozContextPrivate::recvObserve, this, &QMozContext::recvObserve);
}

void QMozContext::setProfile(const QString &profilePath)
{
    d->mRuntime->embedLiteApp()->SetProfilePath(
            !profilePath.isEmpty() ? profilePath.toUtf8().data() : nullptr);
}

QMozContext::~QMozContext()
{
}

void QMozContext::addComponentManifest(const QString &manifestPath)
{
    if (!d->mRuntime->embedLiteApp())
        return;
    d->mRuntime->embedLiteApp()->AddManifestLocation(
            manifestPath.toUtf8().data());
}

void QMozContext::addObserver(const QString &aTopic)
{
    std::string topic = aTopic.toStdString();
    uint &count = d->mObservers[topic];

    // Zero-initialized by default
    ++count;
    // Don't add observers that were already added
    if ((count == 1) && d->IsInitialized()) {
        d->mRuntime->embedLiteApp()->AddObserver(topic.c_str());
    }
}

void QMozContext::removeObserver(const QString &aTopic)
{
    std::string topic = aTopic.toStdString();
    uint &count = d->mObservers[topic];

    if (count > 0) {
        // Only remove observers that have no interested listeners
        --count;
        if (count == 0) {
            d->mObservers.erase(topic);
            if (d->IsInitialized()) {
                d->mRuntime->embedLiteApp()->RemoveObserver(topic.c_str());
            }
        }
    } else {
        qWarning() << "Observer" << aTopic << "wasn't added so can't be removed";
    }
}

void QMozContext::addObservers(const std::vector<std::string> &aObserversList)
{
    std::vector<std::string> observersList;

    // Don't add observers that were already added
    for (const std::string &topic : aObserversList) {
        uint &count = d->mObservers[topic];
        ++count;
        if (count == 1) {
            observersList.push_back(topic);
        }
    }

    if (d->IsInitialized()) {
        d->mRuntime->embedLiteApp()->AddObservers(observersList);
    }
}

void QMozContext::removeObservers(const std::vector<std::string> &aObserversList)
{
    std::vector<std::string> observersList;

    // Only remove observers that have no interested listeners
    for (const std::string &topic : aObserversList) {
        uint &count = d->mObservers[topic];
        if (count > 0) {
            --count;
            if (count == 0) {
                d->mObservers.erase(topic);
                observersList.push_back(topic);
            }
        } else {
            qWarning() << "Observer" << QString::fromStdString(topic) << "wasn't added so can't be removed";
        }
    }

    if (d->IsInitialized()) {
        d->mRuntime->embedLiteApp()->RemoveObservers(observersList);
    }
}

void QMozContext::notifyObservers(const QString &topic, const QString &value)
{
    if (!d->IsInitialized()) {
        qWarning() << "Trying to notify observer before context initialized.";
        return;
    }

    d->mRuntime->embedLiteApp()->SendObserve(
            topic.toUtf8().data(), (const char16_t*)value.constData());
}

void QMozContext::notifyObservers(const QString &topic, const QVariant &value)
{
    if (!d->IsInitialized()) {
        qWarning() << "Trying to notify observers before context initialized.";
        return;
    }

    QJsonDocument doc;
    if (value.userType() == QMetaType::type("QJSValue")) {
        // Qt 5.6 likes to pass a QJSValue
        QJSValue jsValue = qvariant_cast<QJSValue>(value);
        doc = QJsonDocument::fromVariant(jsValue.toVariant());
    } else {
        doc = QJsonDocument::fromVariant(value);
    }

    QByteArray array = doc.toJson();
    d->mRuntime->embedLiteApp()->SendObserve(
            topic.toUtf8().data(),
            (const char16_t*)QString(array).constData());
}

int QMozContext::getNumberOfViews() const
{
    EmbedLiteApp * const app = d->mRuntime->embedLiteApp();
    return app ? app->GetNumberOfViews() : 0;
}

int QMozContext::getNumberOfWindows() const
{
    EmbedLiteApp * const app = d->mRuntime->embedLiteApp();
    return app ? app->GetNumberOfWindows() : 0;
}

QMozContext::TaskHandle QMozContext::PostUITask(QMozContext::TaskCallback cb, void *data, int timeout)
{
    if (!d->mRuntime->embedLiteApp())
        return nullptr;
    return d->mRuntime->embedLiteApp()->PostTask(cb, data, timeout);
}

QMozContext::TaskHandle QMozContext::PostCompositorTask(QMozContext::TaskCallback cb, void *data, int timeout)
{
    if (!d->mRuntime->embedLiteApp())
        return nullptr;
    return d->mRuntime->embedLiteApp()->PostCompositorTask(cb, data, timeout);
}

void QMozContext::CancelTask(QMozContext::TaskHandle handle)
{
    if (!d->mRuntime->embedLiteApp())
        return;
    d->mRuntime->embedLiteApp()->CancelTask(handle);
}

void QMozContext::runEmbedding(int aDelay)
{
    d->mRuntime->start();
}

bool QMozContext::isInitialized() const
{
    return d->mInitialized;
}

EmbedLiteApp *QMozContext::GetApp()
{
    return d->mRuntime->embedLiteApp();
}

void QMozContext::stopEmbedding()
{
    static const char stopActiveProperty[] =
            "_qmozStopEmbeddingActive";
    static const char stopAgainProperty[] =
            "_qmozStopEmbeddingAgain";
    // Public QMozContext wrappers share QMozContextPrivate and its runtime.
    // Coordinate chrome ownership and re-entrant stops through the canonical
    // wrapper regardless of which valid wrapper receives this call.
    QMozContext * const coordinator = QMozContext::instance();
    if (coordinator->property(stopActiveProperty).toBool()) {
        coordinator->setProperty(stopAgainProperty, true);
        return;
    }
    coordinator->setProperty(stopActiveProperty, true);

    bool waitingForWindows = false;
    const bool hasLegacyWindow = registeredWindow();
    const bool hasChromeWindows =
            QtMoz::hasTrackedChromeWindows(coordinator);
    if (hasLegacyWindow || hasChromeWindows) {
        connect(coordinator, &QMozContext::lastWindowDestroyed,
                coordinator, &QMozContext::stopEmbedding,
                Qt::UniqueConnection);
    }
    if (hasLegacyWindow) {
        d->destroyWindow();
        waitingForWindows = true;
    }
    if (hasChromeWindows
            && QtMoz::releaseTrackedChromeWindows(coordinator)) {
        waitingForWindows = true;
    }
    if (!waitingForWindows) {
        disconnect(coordinator, &QMozContext::lastWindowDestroyed,
                   coordinator, &QMozContext::stopEmbedding);
        d->mRuntime->stop();
    }
    coordinator->setProperty(stopActiveProperty, false);
    if (coordinator->property(stopAgainProperty).toBool()) {
        coordinator->setProperty(stopAgainProperty, false);
        if (waitingForWindows) {
            QTimer::singleShot(
                    0, coordinator, &QMozContext::stopEmbedding);
        }
    }
}

quint32 QMozContext::createView(const quint32 &parentId, const uintptr_t &parentBrowsingContext, const bool hidden)
{
    return d->mViewCreator ? d->mViewCreator->createView(parentId, parentBrowsingContext, hidden) : 0;
}

void QMozContext::setIsAccelerated(bool aIsAccelerated)
{
    if (!d->mRuntime->embedLiteApp())
        return;

    d->mRuntime->embedLiteApp()->SetIsAccelerated(aIsAccelerated);
}

bool QMozContext::isAccelerated() const
{
    if (!d->mRuntime->embedLiteApp())
        return false;
    return d->mRuntime->embedLiteApp()->IsAccelerated();
}

void QMozContext::registerWindow(QMozWindow *window)
{
    if (window != d->mMozWindow) {
        d->destroyWindow();
    }
    d->mMozWindow = window;
}

QMozWindow *QMozContext::registeredWindow() const
{
    return d->mMozWindow.data();
}

void QMozContext::notifyFirstUIInitialized()
{
    static bool sCalledOnce = false;
    if (!sCalledOnce) {
        d->mRuntime->embedLiteApp()->SendObserve("final-ui-startup", nullptr);
        sCalledOnce = true;
    }
}

void QMozContext::setViewCreator(QMozViewCreator *viewCreator)
{
    d->mViewCreator = viewCreator;
}
