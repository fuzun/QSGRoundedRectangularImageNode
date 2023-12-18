/*
 * MIT License
 *
 * Copyright (c) 2023 Fatih Uzunoglu <fuzun54@outlook.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "qsgroundedrectangularimagenode.hpp"

#include <QSGTextureMaterial>
#include <QSGOpaqueTextureMaterial>

#include <QCache>
#include <QVector>
#include <QPainterPath>

template<class T>
T QSGRoundedRectangularImageNode::material_cast(QSGMaterial* const material)
{
#ifdef NDEBUG
    return static_cast<T>(object);
#else
    const auto ret = dynamic_cast<T>(material);
    assert(ret); // incompatible material type
    return ret;
#endif
}

QSGRoundedRectangularImageNode::QSGRoundedRectangularImageNode()
{
    setFlags(QSGGeometryNode::OwnsMaterial |
             QSGGeometryNode::OwnsOpaqueMaterial |
             QSGGeometryNode::OwnsGeometry);

    setMaterial(new QSGTextureMaterial);
    setOpaqueMaterial(new QSGOpaqueTextureMaterial);

    setSmooth(m_smooth);

     // Useful for debugging:
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QStringLiteral("RoundedRectangularImage"));
#endif
}

QSGTextureMaterial *QSGRoundedRectangularImageNode::material() const
{
    return material_cast<QSGTextureMaterial*>(QSGGeometryNode::material());
}

QSGOpaqueTextureMaterial *QSGRoundedRectangularImageNode::opaqueMaterial() const
{
    return material_cast<QSGOpaqueTextureMaterial*>(QSGGeometryNode::opaqueMaterial());
}

void QSGRoundedRectangularImageNode::setSmooth(const bool smooth)
{
    if (m_smooth == smooth)
        return;

    {
        const enum QSGTexture::Filtering filtering = smooth ? QSGTexture::Linear : QSGTexture::Nearest;
        material()->setFiltering(filtering);
        opaqueMaterial()->setFiltering(filtering);
    }

    {
        const enum QSGTexture::Filtering mipmapFiltering = smooth ? QSGTexture::Linear : QSGTexture::None;
        material()->setMipmapFiltering(mipmapFiltering);
        opaqueMaterial()->setMipmapFiltering(mipmapFiltering);
    }

    markDirty(QSGNode::DirtyMaterial);
}

void QSGRoundedRectangularImageNode::setTexture(const std::shared_ptr<QSGTexture>& texture)
{
    assert(texture);

    {
        const bool wasAtlas = (!m_texture || m_texture->isAtlasTexture());

        m_texture = texture;

        // Unless we operate on atlas textures, it should be
        // fine to not rebuild the geometry
        if (wasAtlas || texture->isAtlasTexture())
            rebuildGeometry(); // Texture coordinate mismatch
    }

    material()->setTexture(texture.get());
    opaqueMaterial()->setTexture(texture.get());

    markDirty(QSGNode::DirtyMaterial);
}

bool QSGRoundedRectangularImageNode::setShape(const Shape& shape)
{
    if (m_shape == shape)
        return false;

    const bool ret = rebuildGeometry(shape);

    if (ret)
        m_shape = shape;

    return ret;
}

bool QSGRoundedRectangularImageNode::rebuildGeometry(const Shape& shape)
{
    QSGGeometry* const geometry = this->geometry();
    QSGGeometry* const rebuiltGeometry = rebuildGeometry(shape,
                                                         geometry,
                                                         m_texture->isAtlasTexture() ? m_texture.get()
                                                                                     : nullptr);
    if (!rebuiltGeometry)
    {
        return false;
    }
    else if (rebuiltGeometry == geometry)
    {
        // Was able to reconstruct old geometry instance
        markDirty(QSGNode::DirtyGeometry);
    }
    else
    {
        // - Dirty bit set implicitly
        // - No need to remove the old geometry
        setGeometry(rebuiltGeometry);
    }

    return true;
}

QSGGeometry* QSGRoundedRectangularImageNode::rebuildGeometry(const Shape& shape,
                                                             QSGGeometry* geometry,
                                                             const QSGTexture* const atlasTexture)
{
    if (!shape.isValid())
        return nullptr;

    int vertexCount;

    QVector<QPointF> *path;
    std::unique_ptr<QVector<QPointF>> upPath;

    if (qFuzzyIsNull(shape.radius))
    {
        // 4 vertices are needed to construct
        // a rectangle using triangle strip.
        vertexCount = 4;
        path = nullptr; // unused
    }
    else
    {
        using SizePair = QPair<qreal, qreal>;
        using ShapePair = QPair<SizePair, qreal>;

        // We could cache QSGGeometry itself, but
        // it would not be really useful for atlas
        // textures.
        static QCache<ShapePair, QVector<QPointF>> paths;

        ShapePair key {{shape.rect.width(), shape.rect.height()}, {shape.radius}};
        if (paths.contains(key))
        {
            // There is no cache manipulation after this point,
            // so path is assumed to be valid within its scope
            path = paths.object(key);
        }
        else
        {
            QPainterPath painterPath;
            painterPath.addRoundedRect(0, 0, key.first.first, key.first.second, key.second, key.second);
            painterPath = painterPath.simplified();

            upPath = std::make_unique<QVector<QPointF>>(painterPath.elementCount());
            path = upPath.get();

            const int elementCount = painterPath.elementCount();
            for (int i = 0; i < elementCount; ++i)
            {
                // QPainterPath is not necessarily compatible with
                // with GPU primitives. However, simplified painter path
                // with rounded rectangle shape consists of painter path
                // elements of types which can be drawn using primitives.
                assert(painterPath.elementAt(i).type == QPainterPath::ElementType::MoveToElement
                       || painterPath.elementAt(i).type == QPainterPath::ElementType::LineToElement);

                // Symmetry based triangulation based on ordered painter path.
                (*path)[i] = (painterPath.elementAt((i % 2) ? (i)
                                                            : (elementCount - i - 1)));
            }

            paths.insert(key, new QVector<QPointF>(*path));
        }

        vertexCount = path->count();
    }

    if (!geometry)
    {
        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), vertexCount);
        geometry->setIndexDataPattern(QSGGeometry::StaticPattern); // Is this necessary? Indexing is not used
        geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
        geometry->setDrawingMode(QSGGeometry::DrawingMode::DrawTriangleStrip);
    }
    else
    {
        // Size check is implicitly done:
        geometry->allocate(vertexCount);

        // Assume the passed geometry is not a stray one.
        // It is possible to check and create a new QSGGeometry
        // if it is incompatible, but it should not be necessary.
        // So instead, just pass QSGGeometry to this function that
        // is either inherently compatible, or that is created by
        // this function.

        // These two are not required for compatibility,
        // but lets still assert them for performance reasons.
        assert(geometry->indexDataPattern() == QSGGeometry::StaticPattern);
        assert(geometry->vertexDataPattern() == QSGGeometry::StaticPattern);

        assert(geometry->drawingMode() == QSGGeometry::DrawingMode::DrawTriangleStrip);
        assert(geometry->attributes() == QSGGeometry::defaultAttributes_TexturedPoint2D().attributes);
        assert(geometry->sizeOfVertex() == QSGGeometry::defaultAttributes_TexturedPoint2D().stride);
    }

    QRectF texNormalSubRect;
    if (atlasTexture)
    {
        // The texture might not be in the atlas, but it is okay.
        texNormalSubRect = atlasTexture->normalizedTextureSubRect();
    }
    else
    {
        // In case no texture is given at all:
        texNormalSubRect = {0.0, 0.0, 1.0, 1.0};
    }

    if (qFuzzyIsNull(shape.radius))
    {
        // Use the helper function to reconstruct the pure rectangular geometry:
        QSGGeometry::updateTexturedRectGeometry(geometry, shape.rect, texNormalSubRect);
    }
    else
    {
        const auto mapToAtlasTexture = [texNormalSubRect] (const QPointF& nPoint) -> QPointF {
            return {texNormalSubRect.x() + (texNormalSubRect.width() * nPoint.x()),
                texNormalSubRect.y() + (texNormalSubRect.height() * nPoint.y())};
        };

        QSGGeometry::TexturedPoint2D* const points = geometry->vertexDataAsTexturedPoint2D();

        const qreal dx = shape.rect.x();
        const qreal dy = shape.rect.y();
        for (int i = 0; i < geometry->vertexCount(); ++i)
        {
            const QPointF& pos = path->at(i);
            QPointF tPos = {pos.x() / shape.rect.width(), pos.y() / shape.rect.height()};
            if (atlasTexture)
                tPos = mapToAtlasTexture(tPos);

            points[i].set(pos.x() + dx, pos.y() + dy, tPos.x(), tPos.y());
        }
    }

    geometry->markIndexDataDirty();
    geometry->markVertexDataDirty();

    return geometry;
}
