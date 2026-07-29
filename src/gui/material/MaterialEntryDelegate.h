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

#ifndef KEEPASSXC_MATERIALENTRYDELEGATE_H
#define KEEPASSXC_MATERIALENTRYDELEGATE_H

#include <QRect>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

namespace Material
{
    /**
     * Draws the entry list rows.
     *
     * A rounded-16 row at theme()->rowHeight(): a 36px circular avatar holding
     * the entry glyph, the title over the username, then the url, health,
     * TOTP, modified and menu columns laid out from the right edge. Hover and
     * selection paint a state layer behind the whole row.
     *
     * The model supplies everything through the roles below; the delegate never
     * touches Entry directly, which keeps it usable for the report and history
     * lists as well.
     */
    class EntryDelegate : public QStyledItemDelegate
    {
        Q_OBJECT

    public:
        enum DataRole
        {
            TitleRole = Qt::UserRole + 100, // QString
            UsernameRole, // QString
            UrlRole, // QString, elided into UrlColumnWidth
            HealthRole, // Material::Health, via QVariant::fromValue()
            TotpRole, // bool, draws the TOTP glyph
            ModifiedRole, // QString, already formatted for display
            SymbolRole // Material Symbols name for the avatar
        };

        static constexpr int AvatarSize = 36;
        static constexpr int UrlColumnWidth = 132;
        static constexpr int HealthColumnWidth = 104;
        static constexpr int ModifiedColumnWidth = 80;
        static constexpr int TotpColumnWidth = 32;
        static constexpr int MenuColumnWidth = 32;
        static constexpr int HealthDotSize = 8;

        explicit EntryDelegate(QObject* parent = nullptr);
        ~EntryDelegate() override;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        bool editorEvent(QEvent* event,
                         QAbstractItemModel* model,
                         const QStyleOptionViewItem& option,
                         const QModelIndex& index) override;

        /** Hit rect of the more_vert button inside a row. */
        QRect menuButtonRect(const QRect& rowRect) const;
        /** Hit rect of the TOTP glyph inside a row; empty when the entry has no TOTP. */
        QRect totpButtonRect(const QRect& rowRect) const;

        /** Hide the url, health or modified columns when the pane is too narrow. */
        void setCompactColumns(bool compact);
        bool compactColumns() const;

    signals:
        void menuRequested(const QModelIndex& index, const QPoint& globalPos);
        void totpRequested(const QModelIndex& index);

    private:
        bool m_compactColumns = false;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALENTRYDELEGATE_H
