/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozexttexture.h"
#include "qmozembedlog.h"
#include "runtime/qmoztexturelease_p.h"

#include <QOpenGLFunctions>

#include <EGL/egl.h>
#include <EGL/eglext.h>

static uint glTextureTarget(QMozTextureTarget textureTarget)
{
    if (textureTarget == QMozTextureTarget::ExternalOES) {
        return GL_TEXTURE_EXTERNAL_OES;
    }
    return GL_TEXTURE_2D;
}

static void updateExternalTextureParameters()
{
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

QMozExtTexture::QMozExtTexture()
{
}

QMozExtTexture::~QMozExtTexture()
{
    const bool released = QtMoz::releaseTexturePlatformFrames(this);
    if (m_textureId != 0 && released) {
        glDeleteTextures(1, &m_textureId);
    } else if (m_textureId != 0) {
        qCCritical(lcEmbedLiteExt)
                << "Leaking texture whose platform frame is still leased";
    }
    QtMoz::finishTexturePlatformFrameDestruction(this);
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

bool QMozExtTexture::usesExternalTexture() const
{
    return m_textureTarget == QMozTextureTarget::ExternalOES;
}

void QMozExtTexture::bind()
{
    if (m_textureId != 0) {
        glBindTexture(glTextureTarget(m_textureTarget), m_textureId);
        if (usesExternalTexture()) {
            updateExternalTextureParameters();
        } else {
            updateBindOptions();
        }
    }
}

bool QMozExtTexture::updateTexture()
{
    static const PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES
            = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    if (!glEGLImageTargetTexture2DOES) {
        return false;
    }

    if (QtMoz::textureUsesPlatformFrames(this)) {
        uint newTextureId = 0;
        QSize newTextureSize;
        QMozTextureTarget newTextureTarget = QMozTextureTarget::Texture2D;
        const bool acquired = QtMoz::acquireTexturePlatformFrame(
                this, [&](const QMozSurfaceFrame &frame) {
            switch (frame.image.textureTarget) {
            case QMozSurfaceTextureTarget::Texture2D:
                newTextureTarget = QMozTextureTarget::Texture2D;
                break;
            case QMozSurfaceTextureTarget::ExternalOES:
                newTextureTarget = QMozTextureTarget::ExternalOES;
                break;
            default:
                return false;
            }

            glGenTextures(1, &newTextureId);
            if (newTextureId == 0) {
                return false;
            }

            const uint textureTarget = glTextureTarget(newTextureTarget);
            glBindTexture(textureTarget, newTextureId);
            glEGLImageTargetTexture2DOES(
                    textureTarget,
                    static_cast<EGLImageKHR>(frame.image.handle));
            if (glGetError() != GL_NO_ERROR) {
                return false;
            }

            if (newTextureTarget == QMozTextureTarget::ExternalOES) {
                updateExternalTextureParameters();
            } else {
                updateBindOptions(true);
            }
            newTextureSize = frame.image.size;
            return true;
        }, [&]() {
            if (newTextureId != 0) {
                glDeleteTextures(1, &newTextureId);
                newTextureId = 0;
            }
        });
        if (!acquired) {
            if (newTextureId != 0) {
                glDeleteTextures(1, &newTextureId);
            }
            return false;
        }

        if (m_textureId != 0) {
            glDeleteTextures(1, &m_textureId);
        }
        m_textureId = newTextureId;
        m_textureSize = newTextureSize;
        m_textureTarget = newTextureTarget;
        Q_EMIT platformFrameAcquired();
        return true;
    }

    bool changed = false;

    // We don't want to keep a pointer to a QMozWindow in the texture as that could be deleted in
    // the main thread ahead of the texture which would be deleted in the render thread so we
    // connect two through a direct signal connection which we can remove when the window is
    // destroyed.
    Q_EMIT withPlatformImage([&](const QMozEGLImage &image) {
        if (image.image && !image.size.isEmpty()) {
            changed = true;

            m_textureSize = image.size;

            if (m_textureTarget != image.textureTarget && m_textureId != 0) {
                glDeleteTextures(1, &m_textureId);
                m_textureId = 0;
            }
            m_textureTarget = image.textureTarget;

            if (m_textureId == 0) {
                glGenTextures(1, &m_textureId);
            }

            const uint textureTarget = glTextureTarget(m_textureTarget);
            glBindTexture(textureTarget, m_textureId);
            glEGLImageTargetTexture2DOES(textureTarget, image.image);
            if (usesExternalTexture()) {
                updateExternalTextureParameters();
            } else {
                updateBindOptions(true);
            }
        }
    });

    return changed;
}
