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

#ifndef KEEPASSXC_MATERIALTOPAPPBAR_H
#define KEEPASSXC_MATERIALTOPAPPBAR_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

class QAbstractButton;
class QHBoxLayout;
class QAction;
class QLabel;
class QMenu;

namespace Material
{
    class IconButton;
    class TonalButton;

    /**
     * The 64px app bar above the tab strip.
     *
     * Title and subtitle on the left, then the filled Save pill and the four
     * round action buttons: command palette, generator, regex builder and
     * notifications. The notification count rides on the notifications button
     * as an error-coloured badge.
     */
    class TopAppBar : public QWidget
    {
        Q_OBJECT

    public:
        explicit TopAppBar(QWidget* parent = nullptr);
        ~TopAppBar() override;

        QString title() const;
        void setTitle(const QString& title);

        /** The 11px line under the title; empty hides it and centres the title. */
        QString subtitle() const;
        void setSubtitle(const QString& subtitle);

        bool isSaveEnabled() const;
        void setSaveEnabled(bool enabled);

        /** Zero clears the badge. */
        int notificationCount() const;
        void setNotificationCount(int count);

        /**
         * Host a search field between the title and the actions, as the
         * reference does: the entry search lives in the app bar, not in the
         * destination. The widget is reparented; nullptr removes it. The bar
         * shows it only while the destination that owns it is current.
         */
        void setSearchWidget(QWidget* search);
        QWidget* searchWidget() const;
        void setSearchVisible(bool visible);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        /**
         * The trailing actions that did not fit and were folded into the
         * overflow menu, in bar order. Empty when every button is on show.
         */
        QStringList overflowedActions() const;
        /** The overflow button; hidden while nothing is folded. */
        QAbstractButton* overflowButton() const;
        QMenu* overflowMenu() const;

    signals:
        void saveRequested();
        void paletteRequested();
        void generatorRequested();
        void regexRequested();
        void notificationsRequested();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void applyTheme();
        /** Re-elide the title and subtitle against the width the layout gave them. */
        void updateLabels();
        /**
         * Fold trailing action buttons into the overflow menu, last first,
         * until the rest fit beside the title (and search) at their minimum
         * widths; unfold them again as room returns. Nothing is ever clipped.
         */
        void relayoutActions();

        QString m_title;
        QString m_subtitle;
        QLabel* m_titleLabel = nullptr;
        QLabel* m_subtitleLabel = nullptr;
        QWidget* m_titleColumn = nullptr;
        QWidget* m_search = nullptr;
        bool m_searchVisible = false;
        QHBoxLayout* m_layout = nullptr;
        TonalButton* m_saveButton = nullptr;
        IconButton* m_paletteButton = nullptr;
        IconButton* m_generatorButton = nullptr;
        IconButton* m_regexButton = nullptr;
        IconButton* m_notificationsButton = nullptr;
        struct ActionSlot
        {
            IconButton* button = nullptr;
            QAction* action = nullptr;
            QString name;
        };
        QList<ActionSlot> m_actions;
        IconButton* m_overflowButton = nullptr;
        QMenu* m_overflowMenu = nullptr;
        int m_folded = 0;
        bool m_relayouting = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALTOPAPPBAR_H
