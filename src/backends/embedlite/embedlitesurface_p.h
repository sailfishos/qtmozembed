/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QTMOZ_EMBEDLITESURFACE_P_H
#define QTMOZ_EMBEDLITESURFACE_P_H

#include "../../runtime/qmozsurface_p.h"

#include <functional>

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
class EmbedLiteWindow;
class EmbedLiteWindowListener;
}
}

namespace QtMoz {

using EmbedLiteWindowCallback = std::function<void(
        mozilla::embedlite::EmbedLiteWindow *)>;

Q_DECL_HIDDEN QSharedPointer<QMozSurface> createEmbedLiteSurface(
        mozilla::embedlite::EmbedLiteApp *app,
        mozilla::embedlite::EmbedLiteWindowListener *listener);
Q_DECL_HIDDEN mozilla::embedlite::EmbedLiteWindow *reserveEmbedLiteSurface(
        const QSharedPointer<QMozSurface> &surface, const QSize &size);
Q_DECL_HIDDEN bool withEmbedLiteWindow(
        const QSharedPointer<QMozSurface> &surface,
        const EmbedLiteWindowCallback &callback);

} // namespace QtMoz

#endif // QTMOZ_EMBEDLITESURFACE_P_H
