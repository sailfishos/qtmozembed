/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmozcontext.h"
#include "qmozenginesettings.h"
#include "testviewcreator.h"
#include "testhelper.h"

#include <stdio.h>
#include <unistd.h>

#include <QGuiApplication>
#include <QtCore/qstring.h>
#include <QTimer>
#include <QtQml>
#include <QQuickView>
#include <QtQuickTest/quicktest.h>

static QObject *testHelperFactory(QQmlEngine *engine, QJSEngine *)
{
    return new TestHelper(engine);
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<TestViewCreator>("QtMozEmbed.Tests", 1, 0, "WebViewCreator");
    qmlRegisterSingletonType<TestHelper>("QtMozEmbed.Tests", 1, 0, "TestHelper",
                                                    testHelperFactory);

    // These components must be loaded before app start
    QString componentPath(DEFAULT_COMPONENTS_PATH);

    QMozContext::instance()->setProfile(QLatin1String("mozembed-testrunner"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("/components") +
                                                  QString("/EmbedLiteBinComponents.manifest"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("/chrome") +
                                                  QString("/EmbedLiteJSScripts.manifest"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("/chrome") +
                                                  QString("/EmbedLiteOverrides.manifest"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("/components") +
                                                  QString("/EmbedLiteJSComponents.manifest"));

    bool contextDestroyed = false;

    QObject::connect(QMozContext::instance(), &QMozContext::lastViewDestroyed,
                     QMozContext::instance(), &QMozContext::stopEmbedding);
    QObject::connect(QMozContext::instance(), &QMozContext::contextDestroyed,
                     [&] {
        contextDestroyed = true;
    });

    int ret = -1;
    QTimer::singleShot(0, [&] {
        QMozContext::instance()->runEmbedding();
        QMozEngineSettings::instance();
        ret = quick_test_main(argc, argv, "qmlmoztestrunner", 0);
    });

    app.exec();

    // in case there are problems in shutdown (tricky business!), avoid making all the
    // tests failing and rather have a separate test for that
    QByteArray fullShutdown = qgetenv("FULL_SHUTDOWN");
    if (fullShutdown == "0" || fullShutdown == "false") {
        qWarning() << "FULL_SHUTDOWN disabled, doing early exit";
        _exit(ret);
    }

    while (!contextDestroyed) {
        QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents;
        QCoreApplication::processEvents(flags, 500);
    }

    return ret;
}
