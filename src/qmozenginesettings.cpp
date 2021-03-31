/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2016 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozenginesettings.h"
#include "qmozenginesettings_p.h"

#include <QStringList>
#include <QTimer>

#include <mozilla/embedlite/EmbedLiteApp.h>

Q_GLOBAL_STATIC(QMozEngineSettings, engineSettingsInstance)
Q_GLOBAL_STATIC(QMozEngineSettingsPrivate, engineSettingsPrivateInstance)

#define IMAGE_LOAD_ACCEPT 1
#define IMAGE_LOAD_DENY 2
// Allowed, originating site only
#define IMAGE_LOAD_DONT_ACCEPT_FOREIGN 3

namespace {
const auto NS_PREF_CHANGED = QStringLiteral("embed:nsPrefChanged");

const auto PREF_PERMISSIONS_DEFAULT_IMAGE = QStringLiteral("permissions.default.image");
const auto PREF_JAVASCRIPT_ENABLED = QStringLiteral("javascript.enabled");
const auto PREF_POPUP_DISABLE_DURING_LOAD = QStringLiteral("dom.disable_open_during_load");
const auto PREF_COOKIE_BEHAVIOR = QStringLiteral("network.cookie.cookieBehavior");
const auto PREF_USE_DOWNLOAD_DIR = QStringLiteral("browser.download.useDownloadDir");
const auto PREF_DOWNLOAD_DIR = QStringLiteral("browser.download.dir");

const QStringList PREF_CHANGED_OBSERVERS = {
    PREF_PERMISSIONS_DEFAULT_IMAGE,
    PREF_JAVASCRIPT_ENABLED,
    PREF_POPUP_DISABLE_DURING_LOAD,
    PREF_COOKIE_BEHAVIOR,
    PREF_USE_DOWNLOAD_DIR,
    PREF_DOWNLOAD_DIR
};
}

QMozEngineSettingsPrivate *QMozEngineSettingsPrivate::instance()
{
    return engineSettingsPrivateInstance();
}

QMozEngineSettingsPrivate::QMozEngineSettingsPrivate(QObject *parent)
    : QObject(parent)
    , mInitialized(false)
    , mJavascriptEnabled(true)
    , mPopupEnabled(false)
    , mCookieBehavior(QMozEngineSettings::AcceptAll)
    , mUseDownloadDir(false)
    , mAutoLoadImages(true)
    , mPixelRatio(1.0)
{

    QMozContext *context = QMozContext::instance();
    if (context->isInitialized()) {
        QTimer::singleShot(0, this, &QMozEngineSettingsPrivate::initialize);
    } else {
        connect(context, &QMozContext::initialized, this, &QMozEngineSettingsPrivate::initialize);
    }
    connect(context, &QMozContext::recvObserve, this, &QMozEngineSettingsPrivate::onObserve);
    context->addObserver(NS_PREF_CHANGED);
}

QMozEngineSettingsPrivate::~QMozEngineSettingsPrivate()
{
}

bool QMozEngineSettingsPrivate::autoLoadImages() const
{
    return mAutoLoadImages;
}

void QMozEngineSettingsPrivate::setAutoLoadImages(bool enabled)
{
    // 1-Accept, 2-Deny, 3-dontAcceptForeign
    if (mAutoLoadImages != enabled) {
        setPreference(PREF_PERMISSIONS_DEFAULT_IMAGE, QVariant::fromValue<int>(enabled ? IMAGE_LOAD_ACCEPT : IMAGE_LOAD_DENY));
        mAutoLoadImages = enabled;
        Q_EMIT autoLoadImagesChanged();
    }

}

bool QMozEngineSettingsPrivate::javascriptEnabled() const
{
    return mJavascriptEnabled;
}

void QMozEngineSettingsPrivate::setJavascriptEnabled(bool enabled)
{
    if (mJavascriptEnabled != enabled) {
        setPreference(QStringLiteral("javascript.enabled"), QVariant::fromValue<bool>(enabled));
        mJavascriptEnabled = enabled;
        Q_EMIT javascriptEnabledChanged();
    }
}

bool QMozEngineSettingsPrivate::popupEnabled() const
{
    return mPopupEnabled;
}

void QMozEngineSettingsPrivate::setPopupEnabled(bool enabled)
{
    if (mPopupEnabled != enabled) {
        setPreference(PREF_POPUP_DISABLE_DURING_LOAD, QVariant::fromValue<bool>(!enabled));
        mPopupEnabled = enabled;
        Q_EMIT popupEnabledChanged();
    }
}

QMozEngineSettings::CookieBehavior QMozEngineSettingsPrivate::cookieBehavior() const
{
    return mCookieBehavior;
}

void QMozEngineSettingsPrivate::setCookieBehavior(QMozEngineSettings::CookieBehavior cookieBehavior)
{
    if (mCookieBehavior != cookieBehavior) {
        int behavior = cookieBehaviorToInt(cookieBehavior);
        setPreference(PREF_COOKIE_BEHAVIOR, QVariant::fromValue<int>(behavior));
        mCookieBehavior = cookieBehavior;
        Q_EMIT cookieBehaviorChanged();
    }
}

bool QMozEngineSettingsPrivate::useDownloadDir() const
{
    return mUseDownloadDir;
}

void QMozEngineSettingsPrivate::setUseDownloadDir(bool useDownloadDir)
{
    if (mUseDownloadDir != useDownloadDir) {
        setPreference(PREF_USE_DOWNLOAD_DIR, QVariant::fromValue<bool>(useDownloadDir));
        mUseDownloadDir = useDownloadDir;
        Q_EMIT useDownloadDirChanged();
    }
}

QString QMozEngineSettingsPrivate::downloadDir() const
{
    return mDownloadDir;
}

void QMozEngineSettingsPrivate::setDownloadDir(const QString &downloadDir)
{
    if (mDownloadDir != downloadDir) {
        setPreference(PREF_DOWNLOAD_DIR, QVariant::fromValue<QString>(downloadDir));
        mDownloadDir = downloadDir;
        Q_EMIT downloadDirChanged();
    }
}

void QMozEngineSettingsPrivate::setTileSize(const QSize &size)
{
    setPreference(QStringLiteral("layers.tile-width"), QVariant::fromValue<int>(size.width()));
    setPreference(QStringLiteral("layers.tile-height"), QVariant::fromValue<int>(size.height()));
}

void QMozEngineSettingsPrivate::setPixelRatio(qreal ratio)
{
    mPixelRatio = ratio;
    setPreference(QStringLiteral("layout.css.devPixelsPerPx"), QString::number(ratio));
}

qreal QMozEngineSettingsPrivate::pixelRatio() const
{
    return mPixelRatio;
}

void QMozEngineSettingsPrivate::enableProgressivePainting(bool enabled)
{
    setPreference(QStringLiteral("layers.progressive-paint"), QVariant::fromValue<bool>(enabled));
}

void QMozEngineSettingsPrivate::enableLowPrecisionBuffers(bool enabled)
{
    setPreference(QStringLiteral("layers.low-precision-buffer"), QVariant::fromValue<bool>(enabled));
}

void QMozEngineSettingsPrivate::setPreference(const QString &key, const QVariant &value)
{
    qCDebug(lcEmbedLiteExt) << "name:" << key.toUtf8().data() << ", type:" << value.type() << ", user type:" << value.userType();

    if (!isInitialized()) {
        qCDebug(lcEmbedLiteExt) << "Error: context not yet initialized";
        mPreferences.insert(key, value);
        return;
    }

    mozilla::embedlite::EmbedLiteApp *app = QMozContext::instance()->GetApp();

    int type = value.type();
    switch (type) {
    case QMetaType::QString:
    case QMetaType::Float:
    case QMetaType::Double:
        app->SetCharPref(key.toUtf8().data(), value.toString().toUtf8().data());
        break;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        app->SetIntPref(key.toUtf8().data(), value.toInt());
        break;
    case QMetaType::Bool:
        app->SetBoolPref(key.toUtf8().data(), value.toBool());
        break;
    default:
        qCWarning(lcEmbedLiteExt) << "Unknown pref" << key << "value" << value << "type:" << value.type() << "userType:" << value.userType();
    }
}

bool QMozEngineSettingsPrivate::isInitialized() const
{
    return QMozContext::instance()->isInitialized() && mInitialized;
}

QMozEngineSettings::CookieBehavior QMozEngineSettingsPrivate::intToCookieBehavior(int cookieBehavior)
{
    return static_cast<QMozEngineSettings::CookieBehavior>(cookieBehavior);
}

int QMozEngineSettingsPrivate::cookieBehaviorToInt(QMozEngineSettings::CookieBehavior cookieBehavior)
{
    return static_cast<int>(cookieBehavior);
}

void QMozEngineSettingsPrivate::onObserve(const QString &topic, const QVariant &data)
{
    if (topic == NS_PREF_CHANGED) {
        QVariantMap dataMap = data.toMap();
        QString changedPreference = dataMap.value(QStringLiteral("name")).toString();
        QVariant preferenceValue = dataMap.value(QStringLiteral("value"));
        if (PREF_CHANGED_OBSERVERS.contains(changedPreference)) {
            if (changedPreference == PREF_PERMISSIONS_DEFAULT_IMAGE) {
                bool imageLoadingAllowed = !(preferenceValue.toInt() == IMAGE_LOAD_DENY);
                if (mAutoLoadImages != imageLoadingAllowed) {
                    mAutoLoadImages = imageLoadingAllowed;
                    Q_EMIT autoLoadImagesChanged();
                }
            } else if (changedPreference == PREF_JAVASCRIPT_ENABLED) {
                bool jsEnabled = preferenceValue.toBool();
                if (mJavascriptEnabled != jsEnabled) {
                    mJavascriptEnabled = jsEnabled;
                    Q_EMIT javascriptEnabledChanged();
                }
            } else if (changedPreference == PREF_POPUP_DISABLE_DURING_LOAD) {
                bool popupEnabled = !preferenceValue.toBool();
                if (mPopupEnabled != popupEnabled) {
                    mPopupEnabled = popupEnabled;
                    Q_EMIT popupEnabledChanged();
                }
            } else if (changedPreference == PREF_COOKIE_BEHAVIOR) {
                QMozEngineSettings::CookieBehavior behavior = intToCookieBehavior(preferenceValue.toInt());
                if (mCookieBehavior != behavior) {
                    mCookieBehavior = behavior;
                    Q_EMIT cookieBehaviorChanged();
                }
            } else if (changedPreference == PREF_USE_DOWNLOAD_DIR) {
                bool useDownloadDir = preferenceValue.toBool();
                if (mUseDownloadDir != useDownloadDir) {
                    mUseDownloadDir = useDownloadDir;
                    Q_EMIT useDownloadDirChanged();
                }
            } else if (changedPreference == PREF_DOWNLOAD_DIR) {
                QString downloadDir = preferenceValue.toString();
                if (mDownloadDir != downloadDir) {
                    mDownloadDir = downloadDir;
                    Q_EMIT downloadDirChanged();
                }
            }
        }
    }
}

void QMozEngineSettingsPrivate::initialize()
{
    QMozContext *context = QMozContext::instance();

    // Add preference change observers and request preference value.
    for (const auto &pref : PREF_CHANGED_OBSERVERS) {
        QVariantMap data;
        data.insert(QStringLiteral("name"), pref);
        context->notifyObservers(QStringLiteral("embed:addPrefChangedObserver"), data);
    }

    mInitialized = true;
    setDefaultPreferences();

    // Apply initial preferences.
    QMapIterator<QString, QVariant> preferenceIterator(mPreferences);
    while (preferenceIterator.hasNext()) {
        preferenceIterator.next();
        setPreference(preferenceIterator.key(), preferenceIterator.value());
    }
    disconnect(QMozContext::instance(), &QMozContext::initialized, this, &QMozEngineSettingsPrivate::initialize);

    Q_EMIT initialized();
}

void QMozEngineSettingsPrivate::setDefaultPreferences()
{
    mozilla::embedlite::EmbedLiteApp *app = QMozContext::instance()->GetApp();
    if (getenv("DS_UA")) {
        app->SetCharPref("general.useragent.override", "Mozilla/5.0 (X11; Linux x86_64; rv:20.0) Gecko/20130124 Firefox/20.0");
    } else if (getenv("MT_UA")) {
        app->SetCharPref("general.useragent.override", "Mozilla/5.0 (Android; Tablet; rv:20.0) Gecko/20.0 Firefox/20.0");
    } else if (getenv("MP_UA")) {
        app->SetCharPref("general.useragent.override", "Mozilla/5.0 (Android; Mobile; rv:20.0) Gecko/20.0 Firefox/20.0");
    } else if (getenv("CT_UA")) {
        app->SetCharPref("general.useragent.override",
                          "Mozilla/5.0 (Linux; Android 4.0.3; Transformer Prime TF201 Build/IML74K) AppleWebKit/535.19 (KHTML, like Gecko) Tablet Chrome/18.0.1025.166 Safari/535.19");
    } else if (getenv("GB_UA")) {
        app->SetCharPref("general.useragent.override",
                          "Mozilla/5.0 (Meego; NokiaN9) AppleWebKit/534.13 (KHTML, like Gecko) NokiaBrowser/8.5.0 Mobile Safari/534.13");
    } else {
        const char *customUA = getenv("CUSTOM_UA");
        if (customUA) {
            app->SetCharPref("general.useragent.override", customUA);
        }
    }
}

QMozEngineSettings *QMozEngineSettings::instance()
{
    return engineSettingsInstance();
}

QMozEngineSettings::QMozEngineSettings(QObject *parent)
    : QObject(parent)
    , d_ptr(QMozEngineSettingsPrivate::instance())
{
    Q_D(QMozEngineSettings);
    connect(d, &QMozEngineSettingsPrivate::initialized, this, &QMozEngineSettings::initialized);
    connect(d, &QMozEngineSettingsPrivate::autoLoadImagesChanged, this, &QMozEngineSettings::autoLoadImagesChanged);
    connect(d, &QMozEngineSettingsPrivate::javascriptEnabledChanged, this, &QMozEngineSettings::javascriptEnabledChanged);
    connect(d, &QMozEngineSettingsPrivate::popupEnabledChanged, this, &QMozEngineSettings::popupEnabledChanged);
    connect(d, &QMozEngineSettingsPrivate::cookieBehaviorChanged, this, &QMozEngineSettings::cookieBehaviorChanged);
    connect(d, &QMozEngineSettingsPrivate::useDownloadDirChanged, this, &QMozEngineSettings::useDownloadDirChanged);
    connect(d, &QMozEngineSettingsPrivate::downloadDirChanged, this, &QMozEngineSettings::downloadDirChanged);
}

QMozEngineSettings::~QMozEngineSettings()
{

}

bool QMozEngineSettings::isInitialized() const
{
    Q_D(const QMozEngineSettings);
    return d->isInitialized();
}

bool QMozEngineSettings::autoLoadImages() const
{
    Q_D(const QMozEngineSettings);
    return d->autoLoadImages();
}

void QMozEngineSettings::setAutoLoadImages(bool enabled)
{
    Q_D(QMozEngineSettings);
    return d->setAutoLoadImages(enabled);
}

bool QMozEngineSettings::javascriptEnabled() const
{
    Q_D(const QMozEngineSettings);
    return d->javascriptEnabled();
}

void QMozEngineSettings::setJavascriptEnabled(bool enabled)
{
    Q_D(QMozEngineSettings);
    return d->setJavascriptEnabled(enabled);
}

bool QMozEngineSettings::popupEnabled() const
{
    Q_D(const QMozEngineSettings);
    return d->popupEnabled();
}

void QMozEngineSettings::setPopupEnabled(bool enabled)
{
    Q_D(QMozEngineSettings);
    return d->setPopupEnabled(enabled);
}

QMozEngineSettings::CookieBehavior QMozEngineSettings::cookieBehavior() const
{
    Q_D(const QMozEngineSettings);
    return d->cookieBehavior();
}

void QMozEngineSettings::setCookieBehavior(CookieBehavior cookieBehavior)
{
    Q_D(QMozEngineSettings);
    return d->setCookieBehavior(cookieBehavior);
}

bool QMozEngineSettings::useDownloadDir() const
{
    Q_D(const QMozEngineSettings);
    return d->useDownloadDir();
}

void QMozEngineSettings::setUseDownloadDir(bool useDownloadDir)
{
    Q_D(QMozEngineSettings);
    return d->setUseDownloadDir(useDownloadDir);
}

QString QMozEngineSettings::downloadDir() const
{
    Q_D(const QMozEngineSettings);
    return d->downloadDir();
}

void QMozEngineSettings::setDownloadDir(const QString &downloadDir)
{
    Q_D(QMozEngineSettings);
    return d->setDownloadDir(downloadDir);
}

void QMozEngineSettings::setTileSize(const QSize &size)
{
    Q_D(QMozEngineSettings);
    d->setTileSize(size);
}

void QMozEngineSettings::setPixelRatio(qreal pixelRatio)
{
    Q_D(QMozEngineSettings);
    d->setPixelRatio(pixelRatio);
}

qreal QMozEngineSettings::pixelRatio() const
{
    Q_D(const QMozEngineSettings);
    return d->pixelRatio();
}

void QMozEngineSettings::enableProgressivePainting(bool enabled)
{
    Q_D(QMozEngineSettings);
    d->enableProgressivePainting(enabled);
}

void QMozEngineSettings::enableLowPrecisionBuffers(bool enabled)
{
    Q_D(QMozEngineSettings);
    d->enableLowPrecisionBuffers(enabled);
}

void QMozEngineSettings::setPreference(const QString &key, const QVariant &value)
{
    Q_D(QMozEngineSettings);
    d->setPreference(key, value);
}
