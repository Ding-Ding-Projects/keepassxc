/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_MATERIALELEVATION_H
#define KEEPASSXC_MATERIALELEVATION_H

#include <QColor>
#include <QPainterPath>
#include <QRect>

class QGraphicsDropShadowEffect;
class QObject;
class QPainter;

namespace Material
{
    /**
     * Shared painting primitives.
     *
     * Every Material surface in the application is the same shape: a rounded
     * rectangle with an optional hairline border, an optional shadow and an
     * optional state layer on top. These helpers keep that consistent so
     * widgets and item delegates draw identical results.
     */

    /**
     * Rounded rectangle path for @p rect. A radius of Shape::Full (or any value
     * larger than half the shorter side) yields a pill.
     */
    QPainterPath roundedPath(const QRectF& rect, qreal radius);

    /**
     * Fill @p rect with @p fill, then stroke a one pixel @p border inset half a
     * pixel so it stays crisp. An invalid @p border draws no outline; an
     * invalid @p fill draws no background.
     */
    void
    paintSurface(QPainter* painter, const QRect& rect, int radius, const QColor& fill, const QColor& border = QColor());

    /**
     * The el1 / el2 / el3 drop shadow behind @p rect, matching the design's
     * two-layer shadows. @p level is clamped to 1..3.
     */
    void paintShadow(QPainter* painter, const QRect& rect, int radius, int level);

    /**
     * Hover, focus and pressed state layer: @p tint composited over whatever is
     * already there at @p alpha (0..1), clipped to the rounded shape.
     */
    void paintStateLayer(QPainter* painter, const QRect& rect, int radius, const QColor& tint, qreal alpha);

    /**
     * A drop shadow effect matching elevation @p level (1..3), parented to
     * @p parent. Use for real widgets - sheets, FABs and elevated cards - where
     * painting the shadow inside the widget would clip it.
     */
    QGraphicsDropShadowEffect* elevation(int level, QObject* parent);

} // namespace Material

#endif // KEEPASSXC_MATERIALELEVATION_H
