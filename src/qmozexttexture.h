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
#include <functional>

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

    void bind() override;
    bool updateTexture() override;

Q_SIGNALS:
    void getPlatformImage(const std::function<void(void *image, int width, int height)> &callback);

private:
    QRectF m_normalizedTextureSubRect;
    QSize m_textureSize;
    uint m_textureId = 0;

};

#endif
