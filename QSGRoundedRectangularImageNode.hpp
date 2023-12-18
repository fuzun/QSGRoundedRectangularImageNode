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
#ifndef QSGROUNDEDRECTANGULARIMAGENODE_HPP
#define QSGROUNDEDRECTANGULARIMAGENODE_HPP

#include <QSGGeometryNode>

#include <memory>
#include <cmath>

class QSGTextureMaterial;
class QSGOpaqueTextureMaterial;
class QSGTexture;

class QSGRoundedRectangularImageNode : public QSGGeometryNode
{
    template<class T>
    static T material_cast(QSGMaterial* const material);

public:
    struct Shape
    {
        QRectF rect;
        qreal radius = 0.0;

        constexpr bool operator ==(const Shape& b) const
        {
            return (rect == b.rect && qFuzzyCompare(radius, b.radius));
        }

        constexpr bool isValid() const
        {
            return (!rect.isEmpty() && std::isgreaterequal(radius, 0.0));
        }
    };

    QSGRoundedRectangularImageNode();

    // For convenience:
    QSGTextureMaterial* material() const;
    QSGOpaqueTextureMaterial* opaqueMaterial() const;

    void setSmooth(const bool smooth);
    void setTexture(const std::shared_ptr<QSGTexture>& texture);

    inline constexpr Shape shape() const
    {
        return m_shape;
    }

    bool setShape(const Shape& shape);

    inline bool rebuildGeometry()
    {
        return rebuildGeometry(m_shape);
    }

    bool rebuildGeometry(const Shape& shape);

    // Constructs a geometry denoting rounded rectangle using QPainterPath
    static QSGGeometry* rebuildGeometry(const Shape& shape,
                                        QSGGeometry* geometry,
                                        const QSGTexture* const atlasTexture = nullptr);

private:
    std::shared_ptr<QSGTexture> m_texture;
    Shape m_shape;
    bool m_smooth = true;
};

#endif // QSGROUNDEDRECTANGULARIMAGENODE_HPP
