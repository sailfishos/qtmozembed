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
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtQml/QtQml>

#include "qmessagepump.h"
#include "qmozembedlog.h"
#include "qmozcontext.h"
#include "qmozcontext_p.h"
#include "qmozenginesettings.h"
#include "qmozviewcreator.h"
#include "geckoworker.h"
#include "qmozwindow.h"

#include "nsDebug.h"
#include "mozilla/embedlite/EmbedLiteMessagePump.h"
#include "mozilla/embedlite/EmbedLiteView.h"
#include "mozilla/embedlite/EmbedInitGlue.h"

Q_LOGGING_CATEGORY(lcEmbedLiteExt, "org.sailfishos.embedliteext", QtWarningMsg)

using namespace mozilla::embedlite;

Q_GLOBAL_STATIC(QMozContext, mozContextInstance)
Q_GLOBAL_STATIC(QMozContextPrivate, mozContextPrivateInstance)

QMozContextPrivate *QMozContextPrivate::instance()
{
    return mozContextPrivateInstance();
}

QMozContextPrivate::QMozContextPrivate(QObject *parent)
    : QObject(parent)
    , mApp(nullptr)
    , mInitialized(false)
    , mThread(new QThread())
    , mEmbedStarted(false)
    , mQtPump(nullptr)
    , mAsyncContext(!getenv("DISABLE_ASYNC"))
    , mViewCreator(nullptr)
    , mMozWindow(nullptr)
{
    qCDebug(lcEmbedLiteExt) << "Create new Context:" << (void *)this << ", parent:" << (void *)parent << getenv("GRE_HOME");;
    setenv("BUILD_GRE_HOME", BUILD_GRE_HOME, 1);
    // See JB#11625: JSON message are locale aware avoid breaking them
    // This is moved from the sailfish-components-webview.
    setenv("LC_NUMERIC", "C", 1);
    setlocale(LC_NUMERIC, "C");

    // GRE_HOME must be set before QMozContext is initialized. With invoker PWD is empty.
    QByteArray binaryPath = QCoreApplication::applicationDirPath().toLocal8Bit();
    setenv("GRE_HOME", binaryPath.constData(), 1);

    LoadEmbedLite();
    mApp = XRE_GetEmbedLite();
    mApp->SetListener(this);
    mApp->SetIsAccelerated(true);
    if (mAsyncContext) {
        mQtPump = new MessagePumpQt(mApp);
    }
}

QMozContextPrivate::~QMozContextPrivate()
{
}

bool QMozContextPrivate::ExecuteChildThread()
{
    if (!getenv("GECKO_THREAD")) {
        qCDebug(lcEmbedLiteExt) << "Execute in child Native thread:" << (void *)mThread;
        GeckoWorker *worker = new GeckoWorker(mApp);

        connect(mThread, &QThread::started, worker, &GeckoWorker::doWork);
        connect(mThread, &QThread::finished, worker, &GeckoWorker::quit);
        worker->moveToThread(mThread);

        mThread->setObjectName("GeckoWorkerThread");
        mThread->start(QThread::NormalPriority);
        return true;
    }
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
#if defined(GL_PROVIDER_EGL) || defined(GL_PROVIDER_GLX)
    if (mApp->GetRenderType() == EmbedLiteApp::RENDER_AUTO) {
        mApp->SetIsAccelerated(true);
    }
#endif
    mApp->LoadGlobalStyleSheet("chrome://global/content/embedScrollStyles.css", true);

    std::vector<std::string> observersList;
    observersList.reserve(mObservers.size());
    for (const std::pair<std::string, int> &observer : mObservers) {
        if (observer.second > 0) {
            observersList.push_back(observer.first);
        }
    }
    if (observersList.size() > 0) {
        mApp->AddObservers(observersList);
    }

    Q_EMIT initialized();
}

// App Destroyed, and ready to delete and program exit
void QMozContextPrivate::Destroyed()
{
#ifdef DEVELOPMENT_BUILD
    qCInfo(lcEmbedLiteExt);
#endif
    mApp->SetListener(nullptr);

    if (mThread && !mThread->isFinished()) {
        mThread->exit(0);
        mThread->wait();
        mThread = nullptr;
    }

    if (mQtPump) {
        mQtPump->deleteLater();
        mQtPump = nullptr;
    }
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
    return mApp && mInitialized;
}

uint32_t QMozContextPrivate::CreateNewWindowRequested(const uint32_t &chromeFlags,
                                                      EmbedLiteView *aParentView)
{
    qCDebug(lcEmbedLiteExt) << "QtMozEmbedContext new Window requested: parent:" << (void *)aParentView;
    uint32_t viewId = QMozContext::instance()->createView(aParentView ? aParentView->GetUniqueID() : 0);
    return viewId;
}

EmbedLiteMessagePump *QMozContextPrivate::EmbedLoop()
{
    return mQtPump->EmbedLoop();
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
    d->mApp->SetProfilePath(!profilePath.isEmpty() ? profilePath.toUtf8().data() : nullptr);
}

QMozContext::~QMozContext()
{
}

void QMozContext::addComponentManifest(const QString &manifestPath)
{
    if (!d->mApp)
        return;
    d->mApp->AddManifestLocation(manifestPath.toUtf8().data());
}

void QMozContext::addObserver(const QString &aTopic)
{
    std::string topic = aTopic.toStdString();
    uint &count = d->mObservers[topic];

    // Zero-initialized by default
    ++count;
    // Don't add observers that were already added
    if ((count == 1) && d->IsInitialized()) {
        d->mApp->AddObserver(topic.c_str());
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
                d->mApp->RemoveObserver(topic.c_str());
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
    for (const std::string topic : aObserversList) {
        uint &count = d->mObservers[topic];
        ++count;
        if (count == 1) {
            observersList.push_back(topic);
        }
    }

    if (d->IsInitialized()) {
        d->mApp->AddObservers(observersList);
    }
}

void QMozContext::removeObservers(const std::vector<std::string> &aObserversList)
{
    std::vector<std::string> observersList;

    // Only remove observers that have no interested listeners
    for (const std::string topic : aObserversList) {
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
        d->mApp->RemoveObservers(observersList);
    }
}

void QMozContext::notifyObservers(const QString &topic, const QString &value)
{
    if (!d->IsInitialized()) {
        qWarning() << "Trying to notify observer before context initialized.";
        return;
    }

    d->mApp->SendObserve(topic.toUtf8().data(), (const char16_t*)value.constData());
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
    d->mApp->SendObserve(topic.toUtf8().data(), (const char16_t*)QString(array).constData());
}

QMozContext::TaskHandle QMozContext::PostUITask(QMozContext::TaskCallback cb, void *data, int timeout)
{
    if (!d->mApp)
        return nullptr;
    return d->mApp->PostTask(cb, data, timeout);
}

QMozContext::TaskHandle QMozContext::PostCompositorTask(QMozContext::TaskCallback cb, void *data, int timeout)
{
    if (!d->mApp)
        return nullptr;
    return d->mApp->PostCompositorTask(cb, data, timeout);
}

void QMozContext::CancelTask(QMozContext::TaskHandle handle)
{
    if (!d->mApp)
        return;
    d->mApp->CancelTask(handle);
}

void QMozContext::runEmbedding(int aDelay)
{
    if (!d->mEmbedStarted) {
        d->mEmbedStarted = true;
        if (d->mAsyncContext) {
            d->mApp->StartWithCustomPump(EmbedLiteApp::EMBED_THREAD, d->EmbedLoop());
        } else {
            d->mApp->Start(EmbedLiteApp::EMBED_THREAD);
            d->mEmbedStarted = false;
        }
    }
}

bool QMozContext::isInitialized() const
{
    return d->mInitialized;
}

EmbedLiteApp *QMozContext::GetApp()
{
    return d->mApp;
}

void QMozContext::setPixelRatio(float ratio)
{
    qCWarning(lcEmbedLiteExt) << "QMozContext::setPixelRatio is deprecated and will be removed 1st of December 2016. Use QMozEngineSettings::setPixelRatio instead.";
    QMozEngineSettings::instance()->setPixelRatio(ratio);
}

float QMozContext::pixelRatio() const
{
    qCWarning(lcEmbedLiteExt) << "QMozContext::pixelRatio is deprecated and will be removed 1st of December 2016. Use QMozEngineSettings::pixelRatio instead.";
    return QMozEngineSettings::instance()->pixelRatio();
}

void QMozContext::stopEmbedding()
{
    if (registeredWindow()) {
        connect(this, &QMozContext::lastWindowDestroyed, this, &QMozContext::stopEmbedding);
        d->mMozWindow.reset();
    } else {
        GetApp()->Stop();
    }
}

quint32 QMozContext::createView(const quint32 &parentId)
{
    return d->mViewCreator ? d->mViewCreator->createView(parentId) : 0;
}

void QMozContext::setIsAccelerated(bool aIsAccelerated)
{
    if (!d->mApp)
        return;

    d->mApp->SetIsAccelerated(aIsAccelerated);
}

bool QMozContext::isAccelerated() const
{
    if (!d->mApp)
        return false;
    return d->mApp->IsAccelerated();
}

void QMozContext::registerWindow(QMozWindow *window)
{
    d->mMozWindow.reset(window);
}

QMozWindow *QMozContext::registeredWindow() const
{
    return d->mMozWindow.data();
}

void QMozContext::notifyFirstUIInitialized()
{
    static bool sCalledOnce = false;
    if (!sCalledOnce) {
        d->mApp->SendObserve("final-ui-startup", nullptr);
        sCalledOnce = true;
    }
}

void QMozContext::setViewCreator(QMozViewCreator *viewCreator)
{
    d->mViewCreator = viewCreator;
}
