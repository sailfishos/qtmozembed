/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Jolla Mobile Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QMOZTEXTURELEASE_P_H
#define QMOZTEXTURELEASE_P_H

#include "qmozsurface_p.h"

#include <QtGlobal>

#include <functional>

class QMozExtTexture;
class QMozWindow;
class QQuickWindow;

namespace QtMoz {

using QMozTextureFrameImport =
        std::function<bool(const QMozSurfaceFrame &)>;
using QMozTextureFrameDiscard = std::function<void()>;
using QMozTextureFrameInvalidated =
        std::function<void(QMozExtTexture *)>;

Q_DECL_HIDDEN bool attachTextureFrameLease(
        QMozExtTexture *texture, QMozWindow *window, const void *consumer,
        QQuickWindow *renderWindow,
        const QMozTextureFrameInvalidated &invalidatedCallback);
Q_DECL_HIDDEN bool textureUsesPlatformFrames(QMozExtTexture *texture);
Q_DECL_HIDDEN bool acquireTexturePlatformFrame(
        QMozExtTexture *texture,
        const QMozTextureFrameImport &importCallback,
        const QMozTextureFrameDiscard &discardCallback);
// Transfers texture ownership to an exact-window render-thread cleanup. If
// that scene graph has already disappeared, ownership is retained fail-closed
// rather than allowing Gecko to recycle a possibly sampled frame.
Q_DECL_HIDDEN bool scheduleTextureFrameCleanup(QMozExtTexture *texture);
// Returns false when the importing GL context cannot safely release the
// frame. In that case the hidden lease is deliberately retained.
Q_DECL_HIDDEN bool releaseTexturePlatformFrames(QMozExtTexture *texture);

} // namespace QtMoz

#endif // QMOZTEXTURELEASE_P_H
