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

#include "MaterialShapeMorph.h"

namespace Material
{
    QEasingCurve ShapeMorph::springCurve()
    {
        QEasingCurve c(QEasingCurve::BezierSpline);
        c.addCubicBezierSegment(QPointF(0.2, 0.9), QPointF(0.24, 1.06), QPointF(1.0, 1.0));
        return c;
    }

    void ShapeMorph::morphTo(int radius, int durationMs)
    {
        // TODO: honour the reduced-motion setting AND the platform preference -
        // duration 0, end state still applied.
        setStartValue(this->radius());
        setEndValue(radius);
        setDuration(durationMs);
        setEasingCurve(springCurve());
        start();
    }
} // namespace Material
