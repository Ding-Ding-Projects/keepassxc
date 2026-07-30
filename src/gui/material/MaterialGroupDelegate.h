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

#ifndef KEEPASSXC_MATERIALGROUPDELEGATE_H
#define KEEPASSXC_MATERIALGROUPDELEGATE_H

#include <QRect>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

namespace Material
{
    /**
     * Draws the group sidebar rows.
     *
     * A 40px pill holding the group glyph, the elided label and a right-aligned
     * entry count, indented by depth. The selected row is filled with
     * primaryContainer, hover paints a state layer.
     *
     * In a tree view the option rect already carries the indentation, so
     * DepthRole is only needed when the groups are flattened into a list model.
     */
    class GroupDelegate : public QStyledItemDelegate
    {
        Q_OBJECT

    public:
        enum DataRole
        {
            SymbolRole = Qt::UserRole + 200, // Material Symbols name
            CountRole, // int, -1 hides the count
            DepthRole // int, indent steps for flat models
        };

        static constexpr int RowHeight = 40;
        static constexpr int DefaultIndentStep = 12;

        explicit GroupDelegate(QObject* parent = nullptr);
        ~GroupDelegate() override;

        /** Horizontal offset added per depth step; defaults to DefaultIndentStep. */
        int indentStep() const;
        void setIndentStep(int step);

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    private:
        int m_indentStep = DefaultIndentStep;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALGROUPDELEGATE_H
