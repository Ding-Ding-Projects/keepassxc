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

#ifndef KEEPASSXC_MATERIALSHAPEMORPH_H
#define KEEPASSXC_MATERIALSHAPEMORPH_H

#include <QEasingCurve>
#include <QObject>
#include <QVariantAnimation>

namespace Material
{
    /**
     * Corner-radius interpolation for M3 Expressive press states.
     *
     * Expressive components change SHAPE on interaction, not just colour: a
     * rail indicator at radius 17 goes square-ish at 10 while pressed, a FAB at
     * 28 morphs to 16 as its menu opens, a filled button at Full flattens to 14.
     * The animation is a spring, not an ease - the overshoot is what makes it
     * read as physical rather than as a slow resize.
     *
     * Reduced motion collapses the duration to zero rather than skipping the
     * end state, so the pressed shape is still correct for anyone who cannot
     * see the transition.
     */
    class ShapeMorph : public QVariantAnimation
    {
        Q_OBJECT

    public:
        explicit ShapeMorph(QWidget* target, QObject* parent = nullptr);

        void morphTo(int radius, int durationMs = 200);
        int radius() const;

        /** The spring curve the design uses: cubic-bezier(.2,.9,.24,1.06). */
        static QEasingCurve springCurve();

    protected:
        void updateCurrentValue(const QVariant& value) override;
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALSHAPEMORPH_H
