/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZCHROMEWINDOWREGISTRY_P_H
#define QMOZCHROMEWINDOWREGISTRY_P_H

#include <QtGlobal>

#include "qmozchromewindowshutdown_p.h"

class QMozContext;
class QMozWindow;

namespace QtMoz {

Q_DECL_HIDDEN bool trackChromeWindow(
        QMozContext *context, QMozWindow *window,
        const QMozChromeWindowDrainRequest &drainRequest);
Q_DECL_HIDDEN bool drainTrackedChromeWindow(
        QMozContext *context, QMozWindow *window);
Q_DECL_HIDDEN bool hasTrackedChromeWindows(QMozContext *context);
Q_DECL_HIDDEN bool releaseTrackedChromeWindows(QMozContext *context);

} // namespace QtMoz

#endif // QMOZCHROMEWINDOWREGISTRY_P_H
