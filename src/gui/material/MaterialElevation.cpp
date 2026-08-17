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

#include "MaterialElevation.h"

#include <QGraphicsDropShadowEffect>
#include <QPainter>

namespace Material
{
    namespace
    {
        /** One CSS box-shadow layer: `0 <offset>px <blur>px <spread>px black`. */
        struct ShadowLayer
        {
            qreal blur;
            qreal offset;
            qreal spread;
            int alpha;
        };

        /**
         * The design's --el1 / --el2 / --el3, each a tight key shadow over a
         * wider ambient one. Both alphas are constant across the three levels;
         * only the ambient layer's blur, offset and spread grow.
         */
        struct ElevationTokens
        {
            ShadowLayer key;
            ShadowLayer ambient;
        };

        constexpr int KeyAlpha = 76; // rgba(0, 0, 0, .3)
        constexpr int AmbientAlpha = 38; // rgba(0, 0, 0, .15)

        constexpr ElevationTokens Elevations[] = {
            {{2.0, 1.0, 0.0, KeyAlpha}, {3.0, 1.0, 1.0, AmbientAlpha}},
            {{2.0, 1.0, 0.0, KeyAlpha}, {6.0, 2.0, 2.0, AmbientAlpha}},
            {{3.0, 1.0, 0.0, KeyAlpha}, {8.0, 4.0, 3.0, AmbientAlpha}},
        };

        const ElevationTokens& tokensFor(int level)
        {
            return Elevations[qBound(1, level, 3) - 1];
        }

        /**
         * Concentric hairlines fading outwards stand in for a gaussian blur; the
         * falloff is quadratic so the shadow keeps a defined edge near the shape.
         * A CSS spread just widens the layer, so it adds to the ring count.
         */
        void paintShadowLayer(QPainter* painter, const QRect& rect, int radius, const ShadowLayer& layer)
        {
            const int steps = qMax(1, qRound(layer.blur + layer.spread));
            for (int i = steps; i >= 1; --i) {
                const qreal falloff = 1.0 - static_cast<qreal>(i) / (steps + 1);
                QColor shade(0, 0, 0);
                shade.setAlpha(qRound(layer.alpha * falloff * falloff));
                painter->setPen(QPen(shade, 1.0));
                const QRectF ring = QRectF(rect).adjusted(-i, -i + layer.offset, i, i + layer.offset);
                painter->drawPath(roundedPath(ring, radius + i));
            }
        }
    } // namespace

    QPainterPath roundedPath(const QRectF& rect, qreal radius)
    {
        QPainterPath path;
        if (rect.isEmpty()) {
            return path;
        }

        const qreal limit = qMin(rect.width(), rect.height()) / 2.0;
        const qreal r = qBound(0.0, radius, limit);
        if (r <= 0.0) {
            path.addRect(rect);
        } else {
            path.addRoundedRect(rect, r, r);
        }
        return path;
    }

    void paintSurface(QPainter* painter, const QRect& rect, int radius, const QColor& fill, const QColor& border)
    {
        if (!painter || rect.isEmpty()) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        if (fill.isValid() && fill.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawPath(roundedPath(QRectF(rect), radius));
        }

        if (border.isValid() && border.alpha() > 0) {
            // Half a pixel in, so the hairline lands wholly inside rect instead
            // of straddling its edge and going soft.
            const QRectF inner = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(border, 1.0));
            painter->drawPath(roundedPath(inner, radius - 0.5));
        }

        painter->restore();
    }

    void paintShadow(QPainter* painter, const QRect& rect, int radius, int level)
    {
        if (!painter || rect.isEmpty()) {
            return;
        }

        const ElevationTokens& tokens = tokensFor(level);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(Qt::NoBrush);

        // Widest first, so the tight key layer composites over the ambient one
        // in the same order the CSS declares them.
        paintShadowLayer(painter, rect, radius, tokens.ambient);
        paintShadowLayer(painter, rect, radius, tokens.key);

        painter->restore();
    }

    void paintStateLayer(QPainter* painter, const QRect& rect, int radius, const QColor& tint, qreal alpha)
    {
        if (!painter || rect.isEmpty() || !tint.isValid() || alpha <= 0.0) {
            return;
        }

        QColor layer = tint;
        layer.setAlphaF(static_cast<float>(qBound(0.0, alpha, 1.0) * tint.alphaF()));

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(layer);
        painter->drawPath(roundedPath(QRectF(rect), radius));
        painter->restore();
    }

    QGraphicsDropShadowEffect* elevation(int level, QObject* parent)
    {
        const ElevationTokens& tokens = tokensFor(level);

        // A QGraphicsDropShadowEffect draws one shadow, so the pair collapses
        // into the ambient layer's reach - it is the layer that gives the lift -
        // carrying the key layer's alpha, which the design holds at 30 percent
        // for all three levels instead of ramping it up with the level.
        auto* effect = new QGraphicsDropShadowEffect(parent);
        effect->setBlurRadius(tokens.ambient.blur + tokens.ambient.spread);
        effect->setOffset(0.0, tokens.ambient.offset);
        effect->setColor(QColor(0, 0, 0, tokens.key.alpha));
        return effect;
    }

} // namespace Material
