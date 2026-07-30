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

#ifndef KEEPASSXC_MATERIALSWITCH_H
#define KEEPASSXC_MATERIALSWITCH_H

#include <QAbstractButton>

class QKeyEvent;
class QPropertyAnimation;

namespace Material
{
    /**
     * The Material 3 switch used by every behaviour row in the settings screen.
     *
     * A 52x32 track whose knob grows from 16px to 24px as it travels, with the
     * check glyph fading in once on. The knob position is animated over
     * Duration::Toggle and is exposed as a property so the animation can drive
     * it; setChecked() from code animates exactly like a click does.
     */
    class Switch : public QAbstractButton
    {
        Q_OBJECT

        Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

    public:
        static constexpr int TrackWidth = 52;
        static constexpr int TrackHeight = 32;
        static constexpr int KnobSizeOff = 16;
        static constexpr int KnobSizeOn = 24;

        explicit Switch(QWidget* parent = nullptr);
        ~Switch() override;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        /** Travel of the knob, 0 when off and 1 when on. */
        qreal knobPosition() const;
        void setKnobPosition(qreal position);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void checkStateSet() override;
        void nextCheckState() override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        /** QAbstractButton ignores Return; the design toggles on it too. */
        void keyPressEvent(QKeyEvent* event) override;

    private:
        void animateTo(qreal position);

        QPropertyAnimation* m_animation = nullptr;
        qreal m_knobPosition = 0.0;
        bool m_hovered = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSWITCH_H
