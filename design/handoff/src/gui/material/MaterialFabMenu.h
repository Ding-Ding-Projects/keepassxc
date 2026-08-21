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

#ifndef KEEPASSXC_MATERIALFABMENU_H
#define KEEPASSXC_MATERIALFABMENU_H

#include <QWidget>

namespace Material
{
    /**
     * The staggered menu an extended FAB opens.
     *
     * Items rise on a 40 ms stagger from nearest to furthest, and the FAB
     * itself morphs from radius 28 to 16 while its icon rotates 45 degrees, so
     * the plus becomes a close without swapping icons.
     *
     * Note the metrics: the extended FAB is 56 high at radius 28. An earlier
     * audit attributed those to FilledButton, which is 40 high at radius Full.
     * They are different components and neither should be sized from the other.
     */
    class FabMenu : public QWidget
    {
        Q_OBJECT

    public:
        explicit FabMenu(QWidget* parent = nullptr);

        void addAction(const QString& id, const QString& symbol, const QString& label);
        void setExpanded(bool expanded);
        bool isExpanded() const;

    signals:
        void actionTriggered(const QString& id);
        void expandedChanged(bool expanded);
    };
} // namespace Material

#endif // KEEPASSXC_MATERIALFABMENU_H
