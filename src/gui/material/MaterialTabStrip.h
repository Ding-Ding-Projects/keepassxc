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

#ifndef KEEPASSXC_MATERIALTABSTRIP_H
#define KEEPASSXC_MATERIALTABSTRIP_H

#include <QList>
#include <QRect>
#include <QString>
#include <QWidget>

namespace Material
{
    class IconButton;

    /**
     * The 48px database tab strip below the app bar.
     *
     * Each tab is 38px tall with 12px top corners, holding a glyph, an elided
     * label and a close glyph; the active tab is painted in the surface colour
     * of the content below it so the two read as one sheet. Tabs that no longer
     * fit collapse into a trailing overflow chip that carries their count and
     * opens a menu of the hidden tabs - selecting one from that menu behaves
     * exactly like clicking the tab. Search and add buttons close out the row.
     */
    class TabStrip : public QWidget
    {
        Q_OBJECT

    public:
        explicit TabStrip(QWidget* parent = nullptr);
        ~TabStrip() override;

        /** Append a tab. The first one added becomes current. */
        void addTab(const QString& id, const QString& symbol, const QString& label);
        void removeTab(const QString& id);
        void clear();

        void setTabLabel(const QString& id, const QString& label);
        void setTabSymbol(const QString& id, const QString& symbol);

        QString currentTab() const;
        void setCurrentTab(const QString& id);

        int count() const;

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        void tabSelected(const QString& id);
        void tabCloseRequested(const QString& id);
        void newTabRequested();
        void searchRequested();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        struct Tab
        {
            QString id;
            QString symbol;
            QString label;
            QRect rect;
            QRect closeRect;
            bool visible = true;
        };

        int indexOf(const QString& id) const;
        int indexAt(const QPoint& pos) const;
        void relayout();
        void showOverflowMenu();
        /** Width of the overflow chip when it stands for @p hidden tabs. */
        int overflowWidth(int hidden) const;

        QList<Tab> m_tabs;
        // The overflow chip is painted with the tabs rather than being a child
        // widget: it is tab-shaped, borderless and carries the count badge as
        // trailing content, none of which a stock Chip offers.
        QRect m_overflowRect;
        IconButton* m_searchButton = nullptr;
        IconButton* m_addButton = nullptr;
        int m_currentIndex = -1;
        int m_hoverIndex = -1;
        int m_pressedIndex = -1;
        int m_hiddenCount = 0;
        bool m_pressedClose = false;
        bool m_hoverClose = false;
        bool m_overflowHovered = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALTABSTRIP_H
