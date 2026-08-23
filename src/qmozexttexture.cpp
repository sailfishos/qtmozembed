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

static bool matchesRequiredPlatformFrameSize(
        const QSize &frameSize, const QSize &requiredSize)
{
    if (requiredSize.isEmpty()) {
        return true;
    }
    if (frameSize.isEmpty()) {
        return false;
    }

    // The platform image is in framebuffer pixels while the QQuickItem is in
    // logical pixels. Require the same presentation shape rather than the
    // same scale. The tolerance permits a single-pixel rounding difference
    // on either side of the conversion without admitting a rotated or
    // intermediate-sized frame.
    const qint64 frameWidth = frameSize.width();
    const qint64 frameHeight = frameSize.height();
    const qint64 requiredWidth = requiredSize.width();
    const qint64 requiredHeight = requiredSize.height();
    const qint64 difference = qAbs(
            frameWidth * requiredHeight
            - requiredWidth * frameHeight);
    const qint64 tolerance = qMax(
            qMax(frameWidth, frameHeight),
            qMax(requiredWidth, requiredHeight));
    return difference <= tolerance;
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

void QMozExtTexture::requirePlatformFrame(const QSize &size, quint64 revision)
{
    if (m_requiredPlatformFrameSize != size
            || m_requiredPlatformFrameRevision != revision) {
        m_requiredPlatformFrameSize = size;
        m_requiredPlatformFrameRevision = revision;
        m_platformFrameResetPending = true;
    }
}

void QMozExtTexture::bind()
{
    glBindTexture(glTextureTarget(m_textureTarget), m_textureId);
    if (m_textureId != 0) {
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
        if (m_platformFrameResetPending) {
            // The old image must not be sampled with the new item geometry.
            // Release it on its render context before deleting the GL binding,
            // but keep this dynamic texture alive so preprocess() continues
            // consuming frames while the replacement is pending.
            if (!QtMoz::releaseTexturePlatformFrames(this)) {
                return false;
            }
            if (m_textureId != 0) {
                glDeleteTextures(1, &m_textureId);
                m_textureId = 0;
            }
            m_textureSize = QSize();
            m_platformFrameResetPending = false;
        }

        uint newTextureId = 0;
        QSize newTextureSize;
        QMozTextureTarget newTextureTarget = QMozTextureTarget::Texture2D;
        bool requiredSize = false;
        const bool acquired = QtMoz::acquireTexturePlatformFrame(
                this, [&](const QMozSurfaceFrame &frame) {
            newTextureSize = frame.image.size;
            requiredSize = matchesRequiredPlatformFrameSize(
                    newTextureSize, m_requiredPlatformFrameSize);
            if (!requiredSize) {
                // Successfully acquire the stale-sized token so it can be
                // released below. Rejecting it from this callback would put
                // the same token back into the pending slot indefinitely.
                if (m_textureId != 0) {
                    glDeleteTextures(1, &m_textureId);
                    m_textureId = 0;
                }
                m_textureSize = QSize();
                return true;
            }

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

        if (!requiredSize) {
            // This frame was deliberately never sampled. Completing its
            // acquire/release cycle lets Gecko retire it and advertise the
            // next coalesced frame instead of pinning a stale Ready token.
            if (!QtMoz::releaseTexturePlatformFrames(this)) {
                qCCritical(lcEmbedLiteExt)
                        << "Failed to release rejected platform frame"
                        << newTextureSize << "required"
                        << m_requiredPlatformFrameSize;
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
