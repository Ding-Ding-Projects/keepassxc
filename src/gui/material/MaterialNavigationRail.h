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

#ifndef KEEPASSXC_MATERIALNAVIGATIONRAIL_H
#define KEEPASSXC_MATERIALNAVIGATIONRAIL_H

#include <QList>
#include <QRect>
#include <QString>
#include <QWidget>

class QVariantAnimation;

namespace Material
{
    class IconButton;

    /**
     * The 88px navigation rail down the left edge of the window.
     *
     * A 56x56 rounded brand tile at the top, then one 66px wide tile per
     * destination - glyph over a 12px label over a 10px sublabel - and a footer
     * with the theme toggle and the lock button. The active tile is filled with
     * primaryContainer; hover paints a state layer.
     *
     * The tiles are painted rather than built from child widgets so the whole
     * rail restyles in one update() when the theme changes.
     */
    class NavigationRail : public QWidget
    {
        Q_OBJECT

    public:
        explicit NavigationRail(QWidget* parent = nullptr);
        ~NavigationRail() override;

        /** Append a destination. The first one added becomes current. */
        void
        addDestination(const QString& id, const QString& symbol, const QString& label, const QString& sublabel = {});

        /** The 10px line under the label, e.g. the entry count of a database. */
        void setSublabel(const QString& id, const QString& sublabel);

        /** A small count drawn over the tile glyph; an empty string clears it. */
        void setBadge(const QString& id, const QString& badge);

        QString currentDestination() const;
        void setCurrentDestination(const QString& id);

        int count() const;
        void setIconsOnly(bool iconsOnly);
        bool iconsOnly() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void destinationActivated(const QString& id);
        void themeToggleRequested();
        void lockRequested();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;

    private:
        struct Destination
        {
            QString id;
            QString symbol;
            QString label;
            QString sublabel;
            QString badge;
            QRect rect;
        };

        int indexOf(const QString& id) const;
        int indexAt(const QPoint& pos) const;
        void relayout();

        /** The band between the brand tile and the footer that tiles live in. */
        QRect tileViewport() const;
        /** Height the whole run of tiles wants, ignoring what is available. */
        int tileRunHeight() const;
        /** Largest scroll offset that still leaves the last tile in view. */
        int maximumScroll() const;
        /** Clamp m_scrollOffset and relayout if it moved. Answers whether it did. */
        bool clampScroll();
        /** Scroll the least amount that brings @p index fully into view. */
        void ensureVisible(int index);

        /** Select @p index and announce it, as a click or an arrow key would. */
        void activate(int index);
        void setHovered(int index);

        QList<Destination> m_destinations;
        IconButton* m_themeButton = nullptr;
        IconButton* m_lockButton = nullptr;
        // The two 180ms cross-fades: one for the active tile, one for the hover
        // state layer. Both blend an outgoing index into an incoming one.
        QVariantAnimation* m_selectAnimation = nullptr;
        QVariantAnimation* m_hoverAnimation = nullptr;
        int m_currentIndex = -1;
        int m_hoverIndex = -1;
        int m_previousIndex = -1;
        int m_previousHoverIndex = -1;
        qreal m_selectProgress = 1.0;
        qreal m_hoverProgress = 1.0;
        /**
         * How far the run of tiles is scrolled up, in pixels. The design is
         * drawn at 920px, where ten destinations fit with room to spare; the
         * window's own minimum is 500px, where they do not. Scrolling is what
         * keeps every destination reachable at that size instead of dropping
         * the ones that overflow.
         */
        int m_scrollOffset = 0;
        bool m_iconsOnly = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALNAVIGATIONRAIL_H
