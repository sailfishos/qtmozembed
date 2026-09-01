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

#include <QtCore/qglobal.h>

#include <functional>

class QMozExtTexture;
class QMozWindow;
class QQuickWindow;

namespace QtMoz {

using QMozTextureFrameImport =
        std::function<bool(const QMozSurfaceFrame &)>;
using QMozTextureFrameDiscard = std::function<void()>;
using QMozTextureCleanupComplete = std::function<void()>;

Q_DECL_HIDDEN quint64 registerTextureFrameConsumer(const void *consumer);
Q_DECL_HIDDEN quint64 textureFrameConsumerId(const void *consumer);
Q_DECL_HIDDEN void unregisterTextureFrameConsumer(
        const void *consumer, quint64 consumerId);
Q_DECL_HIDDEN bool attachTextureFrameLease(
        QMozExtTexture *texture, QMozWindow *window,
        const void *frameConsumer, quint64 consumerId,
        QQuickWindow *renderWindow);
Q_DECL_HIDDEN bool takeTextureFrameInvalidation(quint64 consumerId);
Q_DECL_HIDDEN bool textureUsesPlatformFrames(QMozExtTexture *texture);
Q_DECL_HIDDEN bool acquireTexturePlatformFrame(
        QMozExtTexture *texture,
        const QMozTextureFrameImport &importCallback,
        const QMozTextureFrameDiscard &discardCallback);
// Transfers texture ownership to an exact-window render-thread cleanup. If
// that scene graph has already disappeared, ownership is retained fail-closed
// rather than allowing Gecko to recycle a possibly sampled frame.
Q_DECL_HIDDEN bool scheduleTextureFrameCleanup(
        QMozExtTexture *texture,
        const QMozTextureCleanupComplete &complete =
                QMozTextureCleanupComplete());
Q_DECL_HIDDEN bool scheduleTextureFrameCleanupForConsumer(
        quint64 consumerId,
        const QMozTextureCleanupComplete &complete =
                QMozTextureCleanupComplete());
// Prevents another texture lease from being attached to this consumer and
// waits for every lease generation which existed when the drain began.
Q_DECL_HIDDEN bool drainTextureFramesForConsumer(
        quint64 consumerId,
        const QMozTextureCleanupComplete &complete);
Q_DECL_HIDDEN void finishTextureFrameConsumerDrain(quint64 consumerId);
Q_DECL_HIDDEN bool hasTextureFrameLease(quint64 consumerId);
Q_DECL_HIDDEN void scheduleTextureCleanupComplete(
        const QMozTextureCleanupComplete &complete);
Q_DECL_HIDDEN bool waitForTextureFrameCleanup(
        quint64 consumerId,
        const QMozTextureCleanupComplete &complete);
// Releases the currently imported frame while retaining the texture's lease.
Q_DECL_HIDDEN bool releaseTexturePlatformFrame(QMozExtTexture *texture);
// Returns false when the importing GL context cannot safely release the
// frame. In that case the hidden lease is deliberately retained.
Q_DECL_HIDDEN bool releaseTexturePlatformFrames(QMozExtTexture *texture);
Q_DECL_HIDDEN void finishTexturePlatformFrameDestruction(
        QMozExtTexture *texture);

} // namespace QtMoz

#endif // QMOZTEXTURELEASE_P_H
