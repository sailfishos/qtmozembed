/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "qmozextmaterialnode.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <QtQuick/QSGSimpleMaterial>

#define LOCAL_GL_TEXTURE_EXTERNAL 0x8D65

struct MozExternalTexture {
    GLuint id;
};

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


class MozTextureShader : public QSGSimpleMaterialShader<MozExternalTexture>
{
    QSG_DECLARE_SIMPLE_SHADER(MozTextureShader, MozExternalTexture)

public:

    const char *vertexShader() const
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

    const char *fragmentShader() const
    {
        return  "#extension GL_OES_EGL_image_external : require                     \n"
                "uniform lowp float qt_Opacity;                                     \n"
                "uniform lowp samplerExternalOES texture;                           \n"
                "varying highp vec2 vTexCoord;                                      \n"
                "void main() {                                                      \n"
                "    gl_FragColor = qt_Opacity * texture2D(texture, vTexCoord);     \n"
                "}";
    }

    QList<QByteArray> attributes() const override
    {
        return QList<QByteArray>() << "aVertex" << "aTexCoord";
    }

    void updateState(const MozExternalTexture *texture, const MozExternalTexture *) override
    {
        glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, texture->id);
    }

    void deactivate()
    {
        glBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, 0);
    }

};

void MozExtMaterialNode::update()
{
    updateGeometry(m_size, m_orientation);
}

void MozExtMaterialNode::updateGeometry(const QSize &size, Qt::ScreenOrientation orientation)
{
    // Follow orientation for the sourceRect.
    const QRectF textureRect(0, 1, 1, -1);
    const QRectF sourceRect(0, 0, size.width(), size.height());

    // and then texture coordinates
    switch (orientation) {
    case Qt::LandscapeOrientation:
        updateRectGeometry(geometry(), sourceRect, textureRect.topRight(), textureRect.topLeft(),
                           textureRect.bottomRight(), textureRect.bottomLeft());
        break;
    case Qt::InvertedPortraitOrientation:
        updateRectGeometry(geometry(), sourceRect, textureRect.bottomRight(), textureRect.topRight(),
                           textureRect.bottomLeft(), textureRect.topLeft());
        break;
    case Qt::InvertedLandscapeOrientation:
        updateRectGeometry(geometry(), sourceRect, textureRect.bottomLeft(), textureRect.bottomRight(),
                           textureRect.topLeft(), textureRect.topRight());
        break;
    default:
        // Portrait / PrimaryOrientation
        updateRectGeometry(geometry(), sourceRect, textureRect.topLeft(), textureRect.bottomLeft(),
                           textureRect.topRight(), textureRect.bottomRight());
        break;
    }
    markDirty(QSGNode::DirtyGeometry);
}

MozExtMaterialNode::MozExtMaterialNode()
    : m_id(0)
    , m_orientation(Qt::PortraitOrientation)
{
    setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));

    QSGSimpleMaterial<MozExternalTexture> *material = MozTextureShader::createMaterial();
    material->setFlag(QSGMaterial::Blending, false);
    material->state()->id = 0;
    setMaterial(material);
    setFlags(OwnsMaterial | OwnsGeometry);
}

void
MozExtMaterialNode::newTexture(int id, const QSize &size, int orientation)
{
    m_id = id;
    m_orientation = static_cast<Qt::ScreenOrientation>(orientation);

    // It might happen that after orientation change when compositing is done
    // QuickMozView::updatePaintNode() gets called before a new texture with new
    // geometry has been created. In this case it's safer to reset node's
    // geometry again.
    if (m_size != size && m_size.width() > 0 && m_size.height() > 0) {
        updateGeometry(size, m_orientation);
    }

    m_size = size;
}

// Before the scene graph starts to render, we update to the pending texture
void
MozExtMaterialNode::prepareNode()
{
    if (m_id) {
        MozExternalTexture *texture = static_cast<QSGSimpleMaterial<MozExternalTexture> *>(material())->state();
        texture->id = m_id;
        m_id = 0;
        markDirty(QSGNode::DirtyMaterial);
    }
}
