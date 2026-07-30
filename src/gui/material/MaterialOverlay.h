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

#ifndef KEEPASSXC_MATERIALOVERLAY_H
#define KEEPASSXC_MATERIALOVERLAY_H

#include <QWidget>

class QPropertyAnimation;

namespace Material
{
    /**
     * A modal sheet inside the window: a 32% black scrim over everything, with
     * one rounded-28 sheet centred on top of it.
     *
     * The overlay resizes itself to cover its parent and stays out of the way
     * until openOverlay() is called. Opening runs the 240ms sheetIn transition -
     * the scrim fades while the sheet rises 18px and scales up from .98 - and
     * closing plays it in reverse before hiding.
     *
     * Escape and a click on the scrim close it; a click on the sheet does not.
     * The sheet widget is reparented and owned by the overlay.
     */
    class Overlay : public QWidget
    {
        Q_OBJECT

        Q_PROPERTY(qreal transition READ transition WRITE setTransition)

    public:
        explicit Overlay(QWidget* parent = nullptr);
        ~Overlay() override;

        void setSheetWidget(QWidget* sheet);
        QWidget* sheetWidget() const;

        /** Fixed sheet width, e.g. 560 for the generator. Zero uses the size hint. */
        int sheetWidth() const;
        void setSheetWidth(int width);

        /**
         * Distance from the top of the overlay to the sheet, e.g. 90 for the
         * command palette. A negative value - the default - centres the sheet
         * vertically the way the regex, generator and confirm scrims do.
         */
        int sheetTopMargin() const;
        void setSheetTopMargin(int margin);

        bool closeOnClickOutside() const;
        void setCloseOnClickOutside(bool enabled);

        bool isOpen() const;

        /** Animation progress, 0 when closed and 1 when fully open. */
        qreal transition() const;
        void setTransition(qreal value);

    public slots:
        void openOverlay();
        void closeOverlay();

    signals:
        void opened();
        void closed();

    protected:
        /** Hook for subclasses to refresh their content before the sheet appears. */
        virtual void aboutToOpen();

        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

        /** Re-centre the sheet and apply the current transition offset. */
        void centreSheet();

    private:
        QWidget* m_sheet = nullptr;
        QPropertyAnimation* m_animation = nullptr;
        qreal m_transition = 0.0;
        int m_sheetWidth = 0;
        int m_sheetTopMargin = -1;
        bool m_open = false;
        bool m_closeOnClickOutside = true;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALOVERLAY_H
