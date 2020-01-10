/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozexttexture.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <QDebug>

QMozExtTexture::QMozExtTexture()
{
}

QMozExtTexture::~QMozExtTexture()
{
    if (m_textureId != 0) {
        glDeleteTextures(1, &m_textureId);
    }
}

int QMozExtTexture::textureId() const
{
    return m_textureId;
}

QSize QMozExtTexture::textureSize() const
{
    return m_textureSize;
}

bool QMozExtTexture::hasAlphaChannel() const
{
    return false;
}

bool QMozExtTexture::hasMipmaps() const
{
    return false;
}

QRectF QMozExtTexture::normalizedTextureSubRect() const
{
    return QRectF(0, 0, 1, 1);
}

void QMozExtTexture::bind()
{
    if (m_textureId != 0) {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textureId);
        updateBindOptions();
    }
}

bool QMozExtTexture::updateTexture()
{
    bool changed = false;

    static const PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES
            = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    if (!glEGLImageTargetTexture2DOES) {
        return false;
    }

    // We don't want to keep a pointer to a QMozWindow in the texture as that could be deleted in
    // the main thread ahead of the texture which would be deleted in the render thread so we
    // connect two through a direct signal connection which we can remove when the window is
    // destroyed.
    Q_EMIT getPlatformImage([&](EGLImageKHR image, int width, int height) {
        if (image) {
            changed = true;

            m_textureSize = QSize(width, height);

            if (m_textureId == 0) {
                glGenTextures(1, &m_textureId);
            }

            glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textureId);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
        }
    });

    return changed;
}
