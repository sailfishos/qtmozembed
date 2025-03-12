/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qMozExtMaterialNode_h
#define qMozExtMaterialNode_h

#include <QtQuick/QSGGeometryNode>
#include <QObject>

#include <QSGTextureMaterial>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

class MozMaterialNode : public QSGGeometryNode
{
public:
    MozMaterialNode();
    ~MozMaterialNode();

    QRectF rect() const;
    void setRect(const QRectF &rect);

    Qt::ScreenOrientation orientation() const;
    void setOrientation(Qt::ScreenOrientation orientation);

    QSGTexture *texture() const;
    virtual void setTexture(QSGTexture *texture);

    void preprocess() override;

private:
    QSGGeometry m_geometry { QSGGeometry::defaultAttributes_TexturedPoint2D(), 4 };
    QRectF m_rect { 0, 0, 0, 0 };
    QRectF m_normalizedTextureSubRect { 0, 0, 1, 1 };
    QSGTexture *m_texture = nullptr;
    Qt::ScreenOrientation m_orientation;
    bool m_geometryChanged = true;
    bool m_textureChanged = true;
};

class MozRgbMaterialNode : public MozMaterialNode
{
public:
    MozRgbMaterialNode();
    ~MozRgbMaterialNode();

    void setTexture(QSGTexture *texture) override;

private:
    QSGOpaqueTextureMaterial m_opaqueMaterial;
    QSGTextureMaterial m_material;
};

#if defined(QT_OPENGL_ES_2)

class MozOpaqueExtTextureMaterial : public QSGMaterial
{
public:
    QSGMaterialShader *createShader() const;
    QSGMaterialType *type() const;
    int compare(const QSGMaterial *other) const;

    QSGTexture *texture() const;
    void setTexture(QSGTexture *texture);

private:
    QSGTexture *m_texture = nullptr;
};


class MozExtTextureMaterial : public MozOpaqueExtTextureMaterial
{
public:
    QSGMaterialShader *createShader() const;
    QSGMaterialType *type() const;
};

class MozExtMaterialNode : public MozMaterialNode
{
public:
    MozExtMaterialNode();
    ~MozExtMaterialNode();

    void setTexture(QSGTexture *texture) override;

private:
    MozOpaqueExtTextureMaterial m_opaqueMaterial;
    MozExtTextureMaterial m_material;
};

#endif

#endif /* qMozExtMaterialNode_h */
