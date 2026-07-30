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
        /**
         * The three elevation steps, approximated with a single Qt shadow each:
         * blur radius, downward offset and shadow alpha.
         */
        struct ElevationTokens
        {
            qreal blur;
            qreal offset;
            int alpha;
        };

        constexpr ElevationTokens Elevations[] = {{4.0, 1.0, 60}, {10.0, 2.0, 70}, {16.0, 4.0, 80}};

        const ElevationTokens& tokensFor(int level)
        {
            return Elevations[qBound(1, level, 3) - 1];
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
        const int steps = qMax(1, qRound(tokens.blur));

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(Qt::NoBrush);

        // Concentric hairlines fading outwards stand in for a gaussian blur; the
        // falloff is quadratic so the shadow keeps a defined edge near the shape.
        for (int i = steps; i >= 1; --i) {
            const qreal falloff = 1.0 - static_cast<qreal>(i) / (steps + 1);
            QColor shade(0, 0, 0);
            shade.setAlpha(qRound(tokens.alpha * falloff * falloff));
            painter->setPen(QPen(shade, 1.0));
            const QRectF ring = QRectF(rect).adjusted(-i, -i + tokens.offset, i, i + tokens.offset);
            painter->drawPath(roundedPath(ring, radius + i));
        }

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

        auto* effect = new QGraphicsDropShadowEffect(parent);
        effect->setBlurRadius(tokens.blur);
        effect->setOffset(0.0, tokens.offset);
        effect->setColor(QColor(0, 0, 0, tokens.alpha));
        return effect;
    }

} // namespace Material
