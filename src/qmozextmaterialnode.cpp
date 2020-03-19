/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozextmaterialnode.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <QtQuick/QSGSimpleMaterial>

#include <QSGDynamicTexture>

static void updateRectGeometry(QSGGeometry *g, const QRectF &rect,
                               const QPointF &topLeft,
                               const QPointF &bottomLeft,
                               const QPointF &topRight,
                               const QPointF &bottomRight)
{
    QSGGeometry::TexturedPoint2D *v = g->vertexDataAsTexturedPoint2D();
    v[0].set(rect.left(), rect.top(), topLeft.x(), topLeft.y());
    v[1].set(rect.left(), rect.bottom(), bottomLeft.x(), bottomLeft.y());
    v[2].set(rect.right(), rect.top(), topRight.x(), topRight.y());
    v[3].set(rect.right(), rect.bottom(), bottomRight.x(), bottomRight.y());
}

MozMaterialNode::MozMaterialNode()
{
    setFlag(UsePreprocess);

    setGeometry(&m_geometry);
}

MozMaterialNode::~MozMaterialNode()
{
}

QRectF MozMaterialNode::rect() const
{
    return m_rect;
}

void MozMaterialNode::setRect(const QRectF &rect)
{
    if (m_rect != rect) {
        m_rect = rect;
        m_geometryChanged = true;
    }
}

Qt::ScreenOrientation MozMaterialNode::orientation() const
{
    return m_orientation;
}

void MozMaterialNode::setOrientation(Qt::ScreenOrientation orientation)
{
    if (m_orientation != orientation) {
        m_orientation = orientation;
        m_geometryChanged = true;
    }
}

QSGTexture *MozMaterialNode::texture() const
{
    return m_texture;
}

void MozMaterialNode::setTexture(QSGTexture *texture)
{
    if (m_texture != texture) {
        m_texture = texture;

        m_textureChanged = true;
    }
}

void MozMaterialNode::preprocess()
{
    if (QSGDynamicTexture *texture = qobject_cast<QSGDynamicTexture *>(m_texture)) {
        m_textureChanged |= texture->updateTexture();
    }

    if (m_textureChanged) {
        m_textureChanged = false;

        markDirty(QSGNode::DirtyMaterial);

        if (m_texture) {
            const QRectF textureSubRect = m_texture->normalizedTextureSubRect();
            if (m_normalizedTextureSubRect != textureSubRect) {
                m_normalizedTextureSubRect = textureSubRect;
                m_geometryChanged = true;
            }
        }
    }

    if (m_geometryChanged) {
        m_geometryChanged = false;

        markDirty(QSGNode::DirtyGeometry);

        // and then texture coordinates
        switch (m_orientation) {
        case Qt::LandscapeOrientation:
            updateRectGeometry(
                        &m_geometry,
                        m_rect,
                        m_normalizedTextureSubRect.topRight(),
                        m_normalizedTextureSubRect.topLeft(),
                        m_normalizedTextureSubRect.bottomRight(),
                        m_normalizedTextureSubRect.bottomLeft());
            break;
        case Qt::InvertedPortraitOrientation:
            updateRectGeometry(
                        &m_geometry,
                        m_rect,
                        m_normalizedTextureSubRect.bottomRight(),
                        m_normalizedTextureSubRect.topRight(),
                        m_normalizedTextureSubRect.bottomLeft(),
                        m_normalizedTextureSubRect.topLeft());
            break;
        case Qt::InvertedLandscapeOrientation:
            updateRectGeometry(
                        &m_geometry,
                        m_rect,
                        m_normalizedTextureSubRect.bottomLeft(),
                        m_normalizedTextureSubRect.bottomRight(),
                        m_normalizedTextureSubRect.topLeft(),
                        m_normalizedTextureSubRect.topRight());
            break;
        default:
            // Portrait / PrimaryOrientation
            updateRectGeometry(
                        &m_geometry,
                        m_rect,
                        m_normalizedTextureSubRect.topLeft(),
                        m_normalizedTextureSubRect.bottomLeft(),
                        m_normalizedTextureSubRect.topRight(),
                        m_normalizedTextureSubRect.bottomRight());
            break;
        }
    }
}

MozRgbMaterialNode::MozRgbMaterialNode()
{
    m_opaqueMaterial.setFlag(QSGMaterial::Blending, false);
    m_material.setFlag(QSGMaterial::Blending, true);

    setOpaqueMaterial(&m_opaqueMaterial);
    setMaterial(&m_material);
}

MozRgbMaterialNode::~MozRgbMaterialNode()
{
}

void MozRgbMaterialNode::setTexture(QSGTexture *texture)
{
    MozMaterialNode::setTexture(texture);

    m_opaqueMaterial.setTexture(texture);
    m_material.setTexture(texture);
}

#if defined(QT_OPENGL_ES_2)

class MozOpaqueExtTextureMaterialShader : public QSGMaterialShader
{
public:
    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect) override;
    char const *const *attributeNames() const override;

protected:
    void initialize() override;
    void activate() override;
    void deactivate() override;

    const char *vertexShader() const override;
    const char *fragmentShader() const override;

private:
    int id_matrix = -1;
    int id_texture = -1;
};

void MozOpaqueExtTextureMaterialShader::updateState(
        const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    QOpenGLShaderProgram * const program = this->program();

    QSGTexture * const newTexture = newEffect
            ? static_cast<const MozOpaqueExtTextureMaterial *>(newEffect)->texture()
            : nullptr;

    QSGTexture * const oldTexture = oldEffect
            ? static_cast<const MozOpaqueExtTextureMaterial *>(oldEffect)->texture()
            : nullptr;

    if (newTexture) {
        if (!oldTexture || (oldTexture->textureId() != newTexture->textureId())) {
            newTexture->bind();
        } else {
            newTexture->updateBindOptions();
        }
    }

    if (state.isMatrixDirty()) {
        program->setUniformValue(id_matrix, state.combinedMatrix());
    }
}

char const *const *MozOpaqueExtTextureMaterialShader::attributeNames() const
{
    static const char * const attributeNames[] = { "aVertex", "aTexCoord", nullptr };

    return attributeNames;
}

void MozOpaqueExtTextureMaterialShader::initialize()
{
    QOpenGLShaderProgram * const program = this->program();

    id_matrix = program->uniformLocation("qt_Matrix");
    id_texture = program->uniformLocation("texture");
}

void MozOpaqueExtTextureMaterialShader::activate()
{
}

void MozOpaqueExtTextureMaterialShader::deactivate()
{
}

const char *MozOpaqueExtTextureMaterialShader::vertexShader() const
{
    return  "attribute highp vec4 aVertex;              \n"
            "attribute highp vec2 aTexCoord;            \n"
            "uniform highp mat4 qt_Matrix;              \n"
            "varying highp vec2 vTexCoord;              \n"
            "void main() {                              \n"
            "    gl_Position = qt_Matrix * aVertex;     \n"
            "    vTexCoord = aTexCoord;                 \n"
            "}";
}

const char *MozOpaqueExtTextureMaterialShader::fragmentShader() const
{
    return  "#extension GL_OES_EGL_image_external : require                     \n"
            "uniform lowp samplerExternalOES texture;                           \n"
            "varying highp vec2 vTexCoord;                                      \n"
            "void main() {                                                      \n"
            "    gl_FragColor = texture2D(texture, vTexCoord);                  \n"
            "}";
}

QSGMaterialShader *MozOpaqueExtTextureMaterial::createShader() const
{
    return new MozOpaqueExtTextureMaterialShader;
}

QSGMaterialType *MozOpaqueExtTextureMaterial::type() const
{
    static QSGMaterialType type;

    return &type;
}

int MozOpaqueExtTextureMaterial::compare(const QSGMaterial *other) const
{
    QSGTexture * const otherTexture = static_cast<const MozOpaqueExtTextureMaterial *>(other)->m_texture;

    return (m_texture ? m_texture->textureId() : 0) - (otherTexture ? otherTexture->textureId() : 0);
}

QSGTexture *MozOpaqueExtTextureMaterial::texture() const
{
    return m_texture;
}

void MozOpaqueExtTextureMaterial::setTexture(QSGTexture *texture)
{
    m_texture = texture;
}

class MozExtTextureMaterialShader : public MozOpaqueExtTextureMaterialShader
{
public:
    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect) override;

protected:
    void initialize() override;

    const char *fragmentShader() const override;

private:
    int id_opacity = -1;
};

void MozExtTextureMaterialShader::updateState(
        const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    MozOpaqueExtTextureMaterialShader::updateState(state, newEffect, oldEffect);

    QOpenGLShaderProgram * const program = this->program();

    if (state.isOpacityDirty()) {
        program->setUniformValue(id_opacity, state.opacity());
    }
}

void MozExtTextureMaterialShader::initialize()
{
    MozOpaqueExtTextureMaterialShader::initialize();

    QOpenGLShaderProgram * const program = this->program();

    id_opacity = program->uniformLocation("qt_Opacity");
}

const char *MozExtTextureMaterialShader::fragmentShader() const
{
    return  "#extension GL_OES_EGL_image_external : require                     \n"
            "uniform lowp float qt_Opacity;                                     \n"
            "uniform lowp samplerExternalOES texture;                           \n"
            "varying highp vec2 vTexCoord;                                      \n"
            "void main() {                                                      \n"
            "    gl_FragColor = qt_Opacity * texture2D(texture, vTexCoord);     \n"
            "}";
}

QSGMaterialShader *MozExtTextureMaterial::createShader() const
{
    return new MozExtTextureMaterialShader;
}

QSGMaterialType *MozExtTextureMaterial::type() const
{
    static QSGMaterialType type;

    return &type;
}

MozExtMaterialNode::MozExtMaterialNode()
{
    m_opaqueMaterial.setFlag(QSGMaterial::Blending, false);
    m_material.setFlag(QSGMaterial::Blending, true);

    setOpaqueMaterial(&m_opaqueMaterial);
    setMaterial(&m_material);
}

MozExtMaterialNode::~MozExtMaterialNode()
{
}

void MozExtMaterialNode::setTexture(QSGTexture *texture)
{
    MozMaterialNode::setTexture(texture);

    m_opaqueMaterial.setTexture(texture);
    m_material.setTexture(texture);
}

#endif
