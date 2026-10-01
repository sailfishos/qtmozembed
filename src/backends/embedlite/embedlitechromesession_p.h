/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QTMOZ_EMBEDLITECHROMESESSION_P_H
#define QTMOZ_EMBEDLITECHROMESESSION_P_H

#include "../../runtime/qmozchromesession_p.h"

#include <QSharedPointer>

class QMozSurface;

namespace QtMoz {

Q_DECL_HIDDEN QSharedPointer<QMozChromeSession> createEmbedLiteChromeSession(
        const QSharedPointer<QMozSurface> &surface);

} // namespace QtMoz

#endif // QTMOZ_EMBEDLITECHROMESESSION_P_H
