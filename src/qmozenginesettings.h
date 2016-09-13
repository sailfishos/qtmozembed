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

#ifndef QMOZENGINE_SETTINGS_H
#define QMOZENGINE_SETTINGS_H

#include <QObject>
#include <QSize>

class QMozEngineSettingsPrivate;

class QMozEngineSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autoLoadImages READ autoLoadImages WRITE setAutoLoadImages NOTIFY autoLoadImagesChanged FINAL)
    Q_PROPERTY(bool javascriptEnabled READ javascriptEnabled WRITE setJavascriptEnabled NOTIFY javascriptEnabledChanged FINAL)

public:
    // C++ API
    static QMozEngineSettings *instance();

    // For QML plugin
    explicit QMozEngineSettings(QObject *parent = 0);
    ~QMozEngineSettings();

    bool isInitialized() const;

    bool autoLoadImages() const;
    void setAutoLoadImages(bool enabled);

    bool javascriptEnabled() const;
    void setJavascriptEnabled(bool enabled);

    void setTileSize(const QSize &size);

    void setPixelRatio(qreal pixelRatio);
    qreal pixelRatio() const;

    void enableProgressivePainting(bool enabled);
    void enableLowPrecisionBuffers(bool enabled);

    // Low-level API to set engine preferences.
    Q_INVOKABLE void setPreference(const QString &key, const QVariant &value);

Q_SIGNALS:
    void autoLoadImagesChanged();
    void javascriptEnabledChanged();
    void initialized();

private:
    QMozEngineSettingsPrivate *d_ptr;
    Q_DISABLE_COPY(QMozEngineSettings)
    Q_DECLARE_PRIVATE(QMozEngineSettings)
};

#endif // QMOZENGINE_SETTINGS_H
