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

#ifndef KEEPASSXC_MATERIALTABOVERFLOW_H
#define KEEPASSXC_MATERIALTABOVERFLOW_H

#include <QWidget>

namespace Material
{
    /**
     * The sheet the tab strip overflows into.
     *
     * Tabs are never clipped silently. Past the available width they collapse
     * into a chevron carrying the hidden count, and this sheet lists every open
     * database - hidden or not - with its own search bar wired to the regex
     * builder, because every search bar must reach the builder.
     *
     * Pinned tabs are excluded from overflow: pinning is the user's statement
     * that a tab should stay reachable at any width, and honouring it only when
     * there is room makes the setting a suggestion.
     */
    class TabOverflow : public QWidget
    {
        Q_OBJECT

    public:
        explicit TabOverflow(QWidget* parent = nullptr);

        void setTabs(const QStringList& labels, int current);
        /** Indices that did not fit and are therefore only reachable here. */
        void setHidden(const QList<int>& indices);

    signals:
        void tabActivated(int index);
        void tabPinned(int index, bool pinned);
        void tabMoved(int from, int to);
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALTABOVERFLOW_H
