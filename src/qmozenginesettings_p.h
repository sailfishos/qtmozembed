/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Raine Makelainen <raine.makelainen@jolla.com>
**
****************************************************************************/
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZENGINE_SETTINGS_P_H
#define QMOZENGINE_SETTINGS_P_H

#include <QObject>
#include "qmozenginesettings.h"
#include "qmozcontext.h"
#include "qmozembedlog.h"

class QMozEngineSettingsPrivate : public QObject
{
    Q_OBJECT

public:
    // C++ API
    static QMozEngineSettingsPrivate *instance();

    // For QML plugin
    explicit QMozEngineSettingsPrivate(QObject *parent = 0);
    ~QMozEngineSettingsPrivate();

    bool autoLoadImages() const;
    void setAutoLoadImages(bool enabled);

    bool javascriptEnabled() const;
    void setJavascriptEnabled(bool enabled);

    void setTileSize(const QSize &size);

    void setPixelRatio(qreal ratio);
    qreal pixelRatio() const;

    void enableProgressivePainting(bool enabled);
    void enableLowPrecisionBuffers(bool enabled);

    void setPreference(const QString &key, const QVariant &value);

    bool isInitialized() const;

public Q_SLOTS:
    void onObserve(const QString &topic, const QVariant &data);
    void setInitialPreferences();

Q_SIGNALS:
    void autoLoadImagesChanged();
    void javascriptEnabledChanged();
    void initialized();

private:
    void setDefaultPreferences();
    QMap<QString, QVariant> mPreferences;
    bool mInitialized;
    bool mJavascriptEnabled;
    qreal mPixelRatio;
};

#endif // QMOZENGINE_SETTINGS_P_H
