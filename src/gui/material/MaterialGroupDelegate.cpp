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

#include "MaterialGroupDelegate.h"

#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QTreeView>
#include <QVariant>

namespace Material
{
    namespace
    {
        constexpr int RowGap = 2; // air between two pills; the pill itself stays 40px
        constexpr int BasePadding = 12;
        constexpr int IconSize = 20;
        constexpr int IconGap = 12;
        constexpr int CountGap = 8;
        constexpr int MinimumLabelWidth = 48;
        constexpr float CountOpacity = 0.70f;

        constexpr int MinimumRowWidth = 2 * BasePadding + IconSize + IconGap + MinimumLabelWidth;

        /**
         * Indent steps for a row. A tree view that indents itself has already
         * offset the row rect, so only flattened models - which carry DepthRole -
         * are indented here.
         */
        int rowDepth(const QStyleOptionViewItem& option, const QModelIndex& index)
        {
            int depth = 0;
            for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent()) {
                ++depth;
            }
            if (depth > 0) {
                const auto* tree = qobject_cast<const QTreeView*>(option.widget);
                return tree && tree->indentation() > 0 ? 0 : depth;
            }
            return qMax(0, index.data(GroupDelegate::DepthRole).toInt());
        }

        QPixmap groupGlyph(const QModelIndex& index, const QColor& tint)
        {
            const QString symbol = index.data(GroupDelegate::SymbolRole).toString();
            if (!symbol.isEmpty()) {
                return Icons::pixmap(symbol, IconSize, tint);
            }
            const QVariant decoration = index.data(Qt::DecorationRole);
            if (decoration.canConvert<QIcon>()) {
                const QIcon icon = decoration.value<QIcon>();
                if (!icon.isNull()) {
                    return icon.pixmap(IconSize, IconSize);
                }
            }
            return Icons::pixmap(QStringLiteral("folder"), IconSize, tint);
        }

        void paintGlyph(QPainter* painter, const QRect& rect, const QPixmap& glyph)
        {
            if (glyph.isNull()) {
                return;
            }
            const QSizeF size = glyph.deviceIndependentSize();
            painter->drawPixmap(
                QPointF(rect.center().x() + 0.5 - size.width() / 2.0, rect.center().y() + 0.5 - size.height() / 2.0),
                glyph);
        }
    } // namespace

    GroupDelegate::GroupDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
    }

    GroupDelegate::~GroupDelegate() = default;

    int GroupDelegate::indentStep() const
    {
        return m_indentStep;
    }

    void GroupDelegate::setIndentStep(int step)
    {
        m_indentStep = qMax(0, step);
    }

    void GroupDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const QRect row = option.rect.adjusted(0, RowGap / 2, 0, -RowGap / 2);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

        QColor fill;
        if (selected) {
            fill = theme()->color(Role::PrimaryContainer);
        } else if (hovered) {
            fill = theme()->color(Role::SurfaceContainerHigh);
        }
        paintSurface(painter, row, Shape::Full, fill);

        const QColor content = theme()->color(selected ? Role::OnPrimaryContainer : Role::OnSurface);
        const int depth = rowDepth(option, index);
        const int indent = depth * m_indentStep + BasePadding;
        QRect available = row.adjusted(indent, 0, -BasePadding, 0);

        const QRect iconRect(available.left(), available.center().y() + 1 - IconSize / 2, IconSize, IconSize);
        paintGlyph(painter, iconRect, groupGlyph(index, content));
        available.setLeft(iconRect.right() + 1 + IconGap);

        const int count = index.data(CountRole).isValid() ? index.data(CountRole).toInt() : -1;
        if (count >= 0) {
            const QFont countFont = theme()->font(TypeRole::LabelMedium);
            const QFontMetrics countMetrics(countFont);
            const QString text = QString::number(count);
            const int width = countMetrics.horizontalAdvance(text);
            const QRect countRect(available.right() - width + 1, available.top(), width, available.height());
            if (countRect.left() - CountGap - available.left() >= MinimumLabelWidth) {
                QColor countColor = content;
                countColor.setAlphaF(CountOpacity);
                painter->setFont(countFont);
                painter->setPen(countColor);
                painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, text);
                available.setRight(countRect.left() - 1 - CountGap);
            }
        }

        // Weight is a property of the tree depth, not of the selection: the root
        // group stays medium and every child stays regular whatever is picked.
        const QFont labelFont = theme()->font(depth == 0 ? TypeRole::LabelLarge : TypeRole::BodyMedium);
        const QFontMetrics labelMetrics(labelFont);
        const QString label = index.data(Qt::DisplayRole).toString();
        painter->setFont(labelFont);
        painter->setPen(content);
        painter->drawText(available,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          labelMetrics.elidedText(label, Qt::ElideRight, qMax(0, available.width())));

        painter->restore();
    }

    QSize GroupDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return {MinimumRowWidth, theme()->rowHeight() + RowGap};
    }

} // namespace Material
