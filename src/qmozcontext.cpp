/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
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

using namespace mozilla::embedlite;

Q_GLOBAL_STATIC(QMozContext, mozContextInstance)
Q_GLOBAL_STATIC(QMozContextPrivate, mozContextPrivateInstance)

QMozContextPrivate *QMozContextPrivate::instance()
{
    return mozContextPrivateInstance();
}

QMozContextPrivate::QMozContextPrivate(QObject *parent)
    : QObject(parent)
    , mApp(NULL)
    , mInitialized(false)
    , mThread(new QThread())
    , mEmbedStarted(false)
    , mQtPump(NULL)
    , mAsyncContext(getenv("USE_ASYNC"))
    , mViewCreator(NULL)
    , mMozWindow(NULL)
{
    LOGT("Create new Context: %p, parent:%p", (void *)this, (void *)qq);
    setenv("BUILD_GRE_HOME", BUILD_GRE_HOME, 1);
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
    // deleting a running thread may result in a crash
    if (!mThread->isFinished()) {
        mThread->exit(0);
        mThread->wait();
    }
    delete mThread;
}

bool QMozContextPrivate::ExecuteChildThread()
{
    if (!getenv("GECKO_THREAD")) {
        LOGT("Execute in child Native thread: %p", (void *)mThread);
        GeckoWorker *worker = new GeckoWorker(mApp);

        connect(mThread, SIGNAL(started()), worker, SLOT(doWork()));
        connect(mThread, SIGNAL(finished()), worker, SLOT(quit()));
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
    if (mThread) {
        LOGT("Stop Native thread: %p", (void *)mThread);
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
    Q_EMIT initialized();
    QListIterator<QString> i(mObserversList);
    while (i.hasNext()) {
        const QString &str = i.next();
        mApp->AddObserver(str.toUtf8().data());
    }
    mObserversList.clear();
}

// App Destroyed, and ready to delete and program exit
void QMozContextPrivate::Destroyed()
{
    LOGT("");
    Q_EMIT contextDestroyed();
    if (mAsyncContext) {
        mQtPump->deleteLater();
    }
}

void QMozContextPrivate::OnObserve(const char *aTopic, const char16_t *aData)
{
    // LOGT("aTopic: %s, data: %s", aTopic, NS_ConvertUTF16toUTF8(aData).get());
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
        // LOGT("mesg:%s, data:%s", aTopic, data.toUtf8().data());
        Q_EMIT recvObserve(aTopic, vdata);
    } else {
        LOGT("parse: s:'%s', err:%s, errLine:%i", data.toUtf8().data(), error.errorString().toUtf8().data(), error.offset);
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

uint32_t QMozContextPrivate::CreateNewWindowRequested(const uint32_t &chromeFlags, const char *uri, const uint32_t &contextFlags,
                                                      EmbedLiteView *aParentView)
{
    LOGT("QtMozEmbedContext new Window requested: parent:%p", (void *)aParentView);
    uint32_t viewId = QMozContext::instance()->createView(QString(uri), aParentView ? aParentView->GetUniqueID() : 0);
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

QMozContext *QMozContext::GetInstance()
{
    qWarning() << "QMozContext::GetInstance() is deprecated and will be removed 1st of December 2016. Use QMozContext::instance() instead.";
    return QMozContext::instance();
}

QMozContext::QMozContext(QObject *parent)
    : QObject(parent)
    , d(QMozContextPrivate::instance())
{
    connect(d, SIGNAL(initialized()), this, SIGNAL(onInitialized()));
    connect(d, SIGNAL(contextDestroyed()), this, SIGNAL(contextDestroyed()));
    connect(d, SIGNAL(lastViewDestroyed()), this, SIGNAL(lastViewDestroyed()));
    connect(d, SIGNAL(lastWindowDestroyed()), this, SIGNAL(lastWindowDestroyed()));
    connect(d, SIGNAL(recvObserve(QString,QVariant)), this, SIGNAL(recvObserve(QString,QVariant)));
}

void QMozContext::setProfile(const QString &profilePath)
{
    d->mApp->SetProfilePath(!profilePath.isEmpty() ? profilePath.toUtf8().data() : NULL);
}

QMozContext::~QMozContext()
{
    if (d->mApp) {
        d->mApp->SetListener(NULL);
    }
    delete d;
}

void QMozContext::sendObserve(const QString &aTopic, const QVariant &value)
{
    qWarning() << "QMozContext::sendObserve is deprecated and will be removed 1st of December 2016. Use QMozContext::notifyObservers instead.";
    notifyObservers(aTopic, value);
}

void QMozContext::sendObserve(const QString &aTopic, const QString &value)
{
    qWarning() << "QMozContext::sendObserve is deprecated and will be removed 1st of December 2016. Use QMozContext::notifyObservers instead.";
    notifyObservers(aTopic, value);
}

void
QMozContext::addComponentManifest(const QString &manifestPath)
{
    if (!d->mApp)
        return;
    d->mApp->AddManifestLocation(manifestPath.toUtf8().data());
}

void
QMozContext::addObserver(const QString &aTopic)
{
    if (!d->IsInitialized()) {
        d->mObserversList.append(aTopic);
        d->mObserversList.removeDuplicates();
        return;
    }

    d->mApp->AddObserver(aTopic.toUtf8().data());
}

void QMozContext::addObservers(const QStringList &aObserversList)
{
    if (!d->IsInitialized()) {
        d->mObserversList.append(aObserversList);
        d->mObserversList.removeDuplicates();
        return;
    }

    nsTArray<nsCString> observersList;
    for (int i = 0; i < aObserversList.size(); i++) {
        observersList.AppendElement(aObserversList.at(i).toUtf8().data());
    }
    d->mApp->AddObservers(observersList);
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

bool
QMozContext::initialized() const
{
    return d->mInitialized;
}

EmbedLiteApp *
QMozContext::GetApp()
{
    return d->mApp;
}

void QMozContext::setPixelRatio(float ratio)
{
    qDebug() << "QMozContext::setPixelRatio is deprecated and will be removed 1st of December 2016. Use QMozEngineSettings::setPixelRatio instead.";
    QMozEngineSettings::instance()->setPixelRatio(ratio);
}

float QMozContext::pixelRatio() const
{
    qDebug() << "QMozContext::pixelRatio is deprecated and will be removed 1st of December 2016. Use QMozEngineSettings::pixelRatio instead.";
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

quint32
QMozContext::createView(const QString &url, const quint32 &parentId)
{
    return d->mViewCreator ? d->mViewCreator->createView(url, parentId) : 0;
}

void
QMozContext::setIsAccelerated(bool aIsAccelerated)
{
    if (!d->mApp)
        return;

    d->mApp->SetIsAccelerated(aIsAccelerated);
}

bool
QMozContext::isAccelerated() const
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

void
QMozContext::setPref(const QString &aName, const QVariant &aPref)
{
    qWarning() << "QMozContext::setPref is deprecated and will be removed 1st of December 2016. Use QMozEngineSettings::setPreference instead.";
    QMozEngineSettings::instance()->setPreference(aName, aPref);
}

void
QMozContext::notifyFirstUIInitialized()
{
    static bool sCalledOnce = false;
    if (!sCalledOnce) {
        d->mApp->SendObserve("final-ui-startup", NULL);
        sCalledOnce = true;
    }
}

void QMozContext::setViewCreator(QMozViewCreator *viewCreator)
{
    d->mViewCreator = viewCreator;
}
