/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2013 - 2019 Jolla Ltd.
 * Copyright (c) 2019 - 2021 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/qqml.h>
#include <QtQml/QQmlEngine>
#include "quickmozview.h"
#include "qmozcontext.h"
#include "qmozenginesettings.h"
#include "qmozscrolldecorator.h"
#include "qmozsecurity.h"

template <typename T> static QObject *singletonApiFactory(QQmlEngine *engine, QJSEngine *)
{
    return new T(engine);
}

class QtMozEmbedPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("Qt5Mozilla"));
        qmlRegisterType<QuickMozView>("Qt5Mozilla", 1, 0, "QmlMozView");
        qmlRegisterSingletonType<QMozContext>("Qt5Mozilla", 1, 0, "QmlMozContext",
                                              singletonApiFactory<QMozContext>);
        qmlRegisterSingletonType<QMozEngineSettings>("Qt5Mozilla", 1, 0, "QMozEngineSettings",
                                              singletonApiFactory<QMozEngineSettings>);

        qmlRegisterUncreatableType<QMozScrollDecorator>("Qt5Mozilla", 1, 0, "QmlMozScrollDecorator", "");
        qmlRegisterUncreatableType<QMozReturnValue>("Qt5Mozilla", 1, 0, "QMozReturnValue", "");
        qmlRegisterType<QMozSecurity>("Qt5Mozilla", 1, 0, "QMozSecurity");
        setenv("EMBED_COMPONENTS_PATH", DEFAULT_COMPONENTS_PATH, 1);
    }
};

#include "main.moc"
