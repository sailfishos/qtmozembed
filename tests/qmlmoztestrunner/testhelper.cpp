/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2021 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "testhelper.h"
#include <QString>

TestHelper::TestHelper(QObject *parent)
    : QObject(parent)
{

}

QString TestHelper::getenv(const QString envVarName) const
{
    return QString(::getenv(envVarName.toUtf8().constData()));
}
