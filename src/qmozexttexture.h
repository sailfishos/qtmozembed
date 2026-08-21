/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef QMOZEXTTEXTURE_H
#define QMOZEXTTEXTURE_H

#include <QSGDynamicTexture>

#include "qmozwindow.h"

class QMozExtTexture : public QSGDynamicTexture
{
    Q_OBJECT
public:
    QMozExtTexture();
    ~QMozExtTexture();

    int textureId() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;

    QRectF normalizedTextureSubRect() const;

    bool usesExternalTexture() const;
    Q_DECL_HIDDEN void requirePlatformFrame(
            const QSize &size, quint64 revision);
    void bind() override;
    bool updateTexture() override;

Q_SIGNALS:
    void withPlatformImage(const QMozEGLImageCallback &callback);
    void platformFrameAcquired();

private:
    QRectF m_normalizedTextureSubRect;
    QSize m_textureSize;
    QSize m_requiredPlatformFrameSize;
    QMozTextureTarget m_textureTarget = QMozTextureTarget::Texture2D;
    uint m_textureId = 0;
    quint64 m_requiredPlatformFrameRevision = 0;
    bool m_platformFrameResetPending = false;
};

#endif
