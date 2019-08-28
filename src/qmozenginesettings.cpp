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

#include <mozilla/embedlite/EmbedLiteApp.h>

Q_GLOBAL_STATIC(QMozEngineSettings, engineSettingsInstance)
Q_GLOBAL_STATIC(QMozEngineSettingsPrivate, engineSettingsPrivateInstance)

#define NS_PREF_CHANGED QStringLiteral("embed:nsPrefChanged")

#define PREF_PERMISSIONS_DEFAULT_IMAGE QStringLiteral("permissions.default.image")
#define PREF_JAVASCRIPT_ENABLED QStringLiteral("javascript.enabled")

#define IMAGE_LOAD_ACCEPT 1
#define IMAGE_LOAD_DENY 2
// Allowed, originating site only
#define IMAGE_LOAD_DONT_ACCEPT_FOREIGN 3

#define PREF_CHANGED_OBSERVERS (QStringList() \
    << PREF_PERMISSIONS_DEFAULT_IMAGE \
    << PREF_JAVASCRIPT_ENABLED);

QMozEngineSettingsPrivate *QMozEngineSettingsPrivate::instance()
{
    return engineSettingsPrivateInstance();
}

QMozEngineSettingsPrivate::QMozEngineSettingsPrivate(QObject *parent)
    : QObject(parent)
    , mInitialized(false)
    , mJavascriptEnabled(true)
    , mAutoLoadImages(true)
    , mPixelRatio(1.0)
{

    QMozContext *context = QMozContext::instance();
    connect(context, &QMozContext::onInitialized, this, &QMozEngineSettingsPrivate::initialize);
    connect(context, &QMozContext::recvObserve, this, &QMozEngineSettingsPrivate::onObserve);
    context->addObserver(NS_PREF_CHANGED);

    // Don't force 16bit color depth
    setPreference(QStringLiteral("gfx.qt.rgb16.force"), QVariant::fromValue<bool>(false));
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
    qCDebug(lcEmbedLiteExt) << "name:" << key.toUtf8().data() << ", type:" << value.type();

    if (!isInitialized()) {
        qCDebug(lcEmbedLiteExt) << "Error: context not yet initialized";
        mPreferences.insert(key, value);
        return;
    }

    mozilla::embedlite::EmbedLiteApp *app = QMozContext::instance()->GetApp();

    switch (value.type()) {
    case QVariant::String:
        app->SetCharPref(key.toUtf8().data(), value.toString().toUtf8().data());
        break;
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
        app->SetIntPref(key.toUtf8().data(), value.toInt());
        break;
    case QVariant::Bool:
        app->SetBoolPref(key.toUtf8().data(), value.toBool());
        break;
    case QVariant::Double:
        if (value.canConvert<int>()) {
            app->SetIntPref(key.toUtf8().data(), value.toInt());
        } else {
            app->SetCharPref(key.toUtf8().data(), value.toString().toUtf8().data());
        }
        break;
    default:
        qCWarning(lcEmbedLiteExt) << "Unknown pref type:" << value.type();
    }
}

bool QMozEngineSettingsPrivate::isInitialized() const
{
    return QMozContext::instance()->initialized() && mInitialized;
}

void QMozEngineSettingsPrivate::onObserve(const QString &topic, const QVariant &data)
{
    if (topic == NS_PREF_CHANGED) {
        QStringList prefChangedObservers = PREF_CHANGED_OBSERVERS;
        QVariantMap dataMap = data.toMap();
        QString changedPreference = dataMap.value(QStringLiteral("name")).toString();
        QVariant preferenceValue = dataMap.value(QStringLiteral("value"));
        if (prefChangedObservers.contains(changedPreference)) {
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
            }
        }
    }
}

void QMozEngineSettingsPrivate::initialize()
{
    QMozContext *context = QMozContext::instance();

    // Add preference change observers.
    QStringList prefChangedObservers = PREF_CHANGED_OBSERVERS;
    QStringList::iterator i;
    for (i = prefChangedObservers.begin(); i != prefChangedObservers.end(); ++i) {
        QVariantMap data;
        data.insert(QStringLiteral("name"), *i);
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
    disconnect(QMozContext::instance(), &QMozContext::onInitialized, this, &QMozEngineSettingsPrivate::initialize);

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
    connect(d, SIGNAL(initialized()), this, SIGNAL(initialized()));
    connect(d, SIGNAL(autoLoadImagesChanged()), this, SIGNAL(autoLoadImagesChanged()));
    connect(d, SIGNAL(javascriptEnabledChanged()), this, SIGNAL(javascriptEnabledChanged()));
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
