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

#ifndef KEEPASSXC_MATERIALSLIDER_H
#define KEEPASSXC_MATERIALSLIDER_H

#include <QSlider>

namespace Material
{
    /**
     * The Material Design 3 slider: a 16px rounded track (primary for the
     * active side, secondaryContainer for the rest) split by a 4px handle bar
     * that thins to 2px while pressed, with a stop indicator at the far end
     * and an optional value label above the handle while it is dragged.
     *
     * It is a QSlider, so every signal, range and step call the screens make
     * keeps working; only the picture and the pointer handling are its own.
     */
    class Slider : public QSlider
    {
        Q_OBJECT

    public:
        explicit Slider(QWidget* parent = nullptr);
        explicit Slider(Qt::Orientation orientation, QWidget* parent = nullptr);

        /** Show the value above the handle while it is pressed; on by default. */
        bool showsValueLabel() const;
        void setShowsValueLabel(bool show);
        /** Text for the value label; the default is the number itself. */
        void setValueLabelSuffix(const QString& suffix);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        QRect trackRect() const;
        int valueAt(const QPoint& position) const;
        int handleCentre() const;
        void init();

        bool m_showsValueLabel = true;
        bool m_hovered = false;
        bool m_dragging = false;
        QString m_suffix;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALSLIDER_H
