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
#include <QGuiApplication>
#include <QtCore/qstring.h>
#include <QTimer>
#include <QtQml>
#include <stdio.h>
#include <QQuickView>
#include <QtQuickTest/quicktest.h>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    setenv("USE_ASYNC", "1", 1);

    qmlRegisterType<TestViewCreator>("qtmozembed.tests", 1, 0, "WebViewCreator");

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
    int ret = -1;
    QTimer::singleShot(0, [&] {
        QMozContext::instance()->runEmbedding();
        ret = quick_test_main(argc, argv, "qmlmoztestrunner", 0);
        QMozContext::instance()->stopEmbedding();
    });
    app.exec();
    return ret;
}
