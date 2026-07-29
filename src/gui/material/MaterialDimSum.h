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

#ifndef KEEPASSXC_MATERIALDIMSUM_H
#define KEEPASSXC_MATERIALDIMSUM_H

#include "MaterialTheme.h"

#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPropertyAnimation;
class QTimer;

namespace Material
{
    class Card;

    /**
     * The dim sum startup surprise.
     *
     * One launch in a hundred, a small card slides into the bottom right corner
     * carrying a randomly drawn dish and its name in both English and
     * Cantonese, holds for six seconds and fades away. It is decoration and
     * nothing else: it never gates startup, never takes focus, never blocks a
     * click, and it stands down entirely on a first run, on an error path,
     * during an update, while a dialog is open, while the desktop is quiet, and
     * whenever GUI_DimSumSurprise is off.
     *
     * Everything it draws is bundled in `:/dimsum`; nothing is fetched.
     */
    class DimSum
    {
    public:
        /** One catalogue entry: the dish in both languages plus its bundled art. */
        struct Dish
        {
            QString english;
            QString cantonese;
            QString asset;

            bool isValid() const;
            /** "Shrimp dumpling · 蝦餃", with the active language's name leading. */
            QString displayName() const;
        };

        /** The bundled dishes, parsed once from `:/dimsum/dimsum.json`. */
        static QVector<Dish> catalogue();

        /** The 1% draw plus every suppression rule. Drawn at most once per launch. */
        static bool shouldShow();

        /** Present the card if this launch drew it. Returns immediately either way. */
        static void showIfDue(QWidget* parent);

        /**
         * Everything shouldShow() checks except the draw, then the card. Returns
         * whether one appeared, so the suppression rules stay observable.
         */
        static bool showNow(QWidget* parent);

        /** True once this launch has spent its one surprise. */
        static bool hasShown();

        /**
         * Stand down for the rest of this launch. Callers on a first run, an
         * error path, an update or any mid-task flow say so through here.
         */
        static void suppress();

    private:
        DimSum() = delete;
    };

    /**
     * The card itself: a rounded-28 elevated surface pinned to the bottom right
     * of its window, carrying the dish art, its name in both languages and a
     * line of copy pitched at the active language's funny level.
     *
     * It slides in over Duration::Long, holds, then fades out - or appears and
     * leaves without motion when the desktop asks for less of it. The widget is
     * transparent to the mouse outside the card, so the window underneath never
     * loses a click, and a click on the card dismisses it.
     */
    class DimSumCard : public QWidget
    {
        Q_OBJECT

        Q_PROPERTY(qreal transition READ transition WRITE setTransition)

    public:
        DimSumCard(const DimSum::Dish& dish, QWidget* parent);
        ~DimSumCard() override;

        /** Animation progress, 0 off-screen and transparent, 1 fully settled. */
        qreal transition() const;
        void setTransition(qreal value);

        /** Slide in and arm the hold timer. */
        void present();

    public slots:
        /** Fade out, then delete. Idempotent. */
        void dismiss();

    protected:
        void paintEvent(QPaintEvent* event) override;
        /** A click on the card dismisses it; one beside it belongs to the window. */
        void mousePressEvent(QMouseEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        /** The card surface inside the widget, with the shadow margin stripped off. */
        QRect cardRect() const;
        /** Park the card against the bottom right corner, offset by the transition. */
        void reposition();
        void applyTheme();

        DimSum::Dish m_dish;
        QPointer<QWidget> m_host;
        Card* m_card = nullptr;
        QLabel* m_artLabel = nullptr;
        QLabel* m_nameLabel = nullptr;
        QLabel* m_captionLabel = nullptr;
        QPropertyAnimation* m_animation = nullptr;
        QTimer* m_holdTimer = nullptr;
        qreal m_transition = 0.0;
        bool m_reducedMotion = false;
        bool m_dismissing = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALDIMSUM_H
