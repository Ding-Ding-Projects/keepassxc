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

#include "MaterialEntryDelegate.h"

#include "MaterialElevation.h"
#include "MaterialIcons.h"
#include "MaterialTheme.h"

#include <QFontMetrics>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QVariant>

namespace Material
{
    namespace
    {
        constexpr int RowInset = 2; // vertical air between two rows
        constexpr int RowPadding = 12;
        constexpr int ColumnGap = 14;
        constexpr int AvatarGlyphSize = 20;
        constexpr int TotpGlyphSize = 18;
        constexpr int MenuGlyphSize = 20;
        constexpr int HealthGap = 6;
        constexpr float SecondaryOpacity = 0.72f;
        constexpr float MenuOpacity = 0.60f;

        // The title column never shrinks past this; the optional columns are
        // dropped one by one instead, starting with the url.
        constexpr int MinimumTextWidth = 120;

        constexpr int MinimumRowWidth = 2 * RowPadding + EntryDelegate::AvatarSize + 2 * ColumnGap + MinimumTextWidth
                                        + EntryDelegate::MenuColumnWidth;

        /** Where every column of one row lands. Empty rects are columns that did not fit. */
        struct RowLayout
        {
            QRect avatar;
            QRect text;
            QRect url;
            QRect health;
            QRect totp;
            QRect modified;
            QRect menu;
        };

        /**
         * Lay a row out from the right edge inwards. The menu button is always
         * placed; every other column is only taken while the title column keeps
         * its minimum width, so a narrow pane sheds columns instead of clipping
         * the title.
         */
        RowLayout layoutRow(const QRect& rowRect, bool compact)
        {
            RowLayout layout;
            QRect content = rowRect.adjusted(RowPadding, 0, -RowPadding, 0);

            layout.avatar = QRect(content.left(),
                                  content.top() + (content.height() - EntryDelegate::AvatarSize) / 2,
                                  EntryDelegate::AvatarSize,
                                  EntryDelegate::AvatarSize);
            content.setLeft(layout.avatar.right() + 1 + ColumnGap);

            auto takeRight = [&content](int width) {
                const QRect column(content.right() - width + 1, content.top(), width, content.height());
                content.setRight(column.left() - 1 - ColumnGap);
                return column;
            };
            auto fits = [&content](int width) { return content.width() - width - ColumnGap >= MinimumTextWidth; };

            layout.menu = takeRight(EntryDelegate::MenuColumnWidth);
            if (!compact && fits(EntryDelegate::ModifiedColumnWidth)) {
                layout.modified = takeRight(EntryDelegate::ModifiedColumnWidth);
            }
            if (fits(EntryDelegate::TotpColumnWidth)) {
                layout.totp = takeRight(EntryDelegate::TotpColumnWidth);
            }
            if (!compact && fits(EntryDelegate::HealthColumnWidth)) {
                layout.health = takeRight(EntryDelegate::HealthColumnWidth);
            }
            if (!compact && fits(EntryDelegate::UrlColumnWidth)) {
                layout.url = takeRight(EntryDelegate::UrlColumnWidth);
            }

            layout.text = content;
            if (layout.text.width() < 0) {
                layout.text.setWidth(0);
            }
            return layout;
        }

        /** Health as the model may express it: the enum, an int or the config string. */
        Health healthOf(const QVariant& value)
        {
            if (!value.isValid()) {
                return Health::Unknown;
            }
            if (value.metaType() == QMetaType::fromType<Health>()) {
                return value.value<Health>();
            }
            if (value.typeId() == QMetaType::QString) {
                return Theme::healthFromString(value.toString());
            }
            bool ok = false;
            const int raw = value.toInt(&ok);
            if (ok && raw >= static_cast<int>(Health::Ok) && raw <= static_cast<int>(Health::Unknown)) {
                return static_cast<Health>(raw);
            }
            return Health::Unknown;
        }

        QString healthLabel(Health health)
        {
            switch (health) {
            case Health::Ok:
                return EntryDelegate::tr("Healthy");
            case Health::Weak:
                return EntryDelegate::tr("Weak");
            case Health::Reused:
                return EntryDelegate::tr("Reused");
            case Health::Breached:
                return EntryDelegate::tr("Breached");
            case Health::Unknown:
                break;
            }
            return EntryDelegate::tr("Unknown");
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

        /** The row glyph: the symbol name if the model has one, else its decoration. */
        QPixmap rowGlyph(const QModelIndex& index, int role, const QString& fallback, int size, const QColor& tint)
        {
            const QString symbol = index.data(role).toString();
            if (!symbol.isEmpty()) {
                return Icons::pixmap(symbol, size, tint);
            }
            const QVariant decoration = index.data(Qt::DecorationRole);
            if (decoration.canConvert<QIcon>()) {
                const QIcon icon = decoration.value<QIcon>();
                if (!icon.isNull()) {
                    return icon.pixmap(size, size);
                }
            }
            return Icons::pixmap(fallback, size, tint);
        }
    } // namespace

    EntryDelegate::EntryDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
    }

    EntryDelegate::~EntryDelegate() = default;

    void EntryDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const QRect row = option.rect.adjusted(0, RowInset, 0, -RowInset);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);

        // An invalid fill leaves the row transparent, which is the resting state.
        QColor fill;
        if (selected) {
            fill = theme()->color(Role::SecondaryContainer);
        } else if (hovered) {
            fill = theme()->color(Role::SurfaceContainerHigh);
        }
        paintSurface(painter, row, Shape::Row, fill);

        const QColor content = theme()->color(selected ? Role::OnSecondaryContainer : Role::OnSurface);
        QColor secondary = content;
        secondary.setAlphaF(SecondaryOpacity);

        const RowLayout layout = layoutRow(row, m_compactColumns);

        painter->setPen(Qt::NoPen);
        painter->setBrush(theme()->color(Role::PrimaryContainer));
        painter->drawEllipse(layout.avatar);
        paintGlyph(
            painter,
            layout.avatar,
            rowGlyph(
                index, SymbolRole, QStringLiteral("key"), AvatarGlyphSize, theme()->color(Role::OnPrimaryContainer)));

        QString title = index.data(TitleRole).toString();
        if (title.isEmpty()) {
            title = index.data(Qt::DisplayRole).toString();
        }
        const QString username = index.data(UsernameRole).toString();

        const QFont titleFont = theme()->font(TypeRole::LabelLarge);
        // The design's secondary lines are 12px regular; the scale's 12px role is medium.
        QFont metaFont = theme()->font(TypeRole::LabelMedium);
        metaFont.setWeight(QFont::Normal);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics metaMetrics(metaFont);

        const bool twoLines =
            !username.isEmpty() && titleMetrics.height() + metaMetrics.height() <= layout.text.height();
        QRect titleRect = layout.text;
        if (twoLines) {
            const int block = titleMetrics.height() + metaMetrics.height();
            titleRect = QRect(layout.text.left(),
                              layout.text.top() + (layout.text.height() - block) / 2,
                              layout.text.width(),
                              titleMetrics.height());
            const QRect userRect(titleRect.left(), titleRect.bottom() + 1, titleRect.width(), metaMetrics.height());
            painter->setFont(metaFont);
            painter->setPen(secondary);
            painter->drawText(userRect,
                              Qt::AlignLeft | Qt::AlignVCenter,
                              metaMetrics.elidedText(username, Qt::ElideRight, userRect.width()));
        }
        painter->setFont(titleFont);
        painter->setPen(content);
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(title, Qt::ElideRight, titleRect.width()));

        if (!layout.url.isEmpty()) {
            painter->setFont(metaFont);
            painter->setPen(secondary);
            painter->drawText(
                layout.url,
                Qt::AlignLeft | Qt::AlignVCenter,
                metaMetrics.elidedText(index.data(UrlRole).toString(), Qt::ElideRight, layout.url.width()));
        }

        if (!layout.health.isEmpty()) {
            const Health health = healthOf(index.data(HealthRole));
            const QColor healthTint = theme()->colors().healthColor(health);
            const QRect dot(
                layout.health.left(), layout.health.center().y() + 1 - HealthDotSize / 2, HealthDotSize, HealthDotSize);
            painter->setPen(Qt::NoPen);
            painter->setBrush(healthTint);
            painter->drawEllipse(dot);

            const QFont healthFont = theme()->font(TypeRole::LabelSmall);
            const QFontMetrics healthMetrics(healthFont);
            const QRect labelRect = layout.health.adjusted(HealthDotSize + HealthGap, 0, 0, 0);
            painter->setFont(healthFont);
            painter->setPen(healthTint);
            painter->drawText(labelRect,
                              Qt::AlignLeft | Qt::AlignVCenter,
                              healthMetrics.elidedText(healthLabel(health), Qt::ElideRight, labelRect.width()));
        }

        if (!layout.totp.isEmpty() && index.data(TotpRole).toBool()) {
            const QColor tint = selected ? content : theme()->color(Role::OnSurfaceVariant);
            paintGlyph(painter, layout.totp, Icons::pixmap(QStringLiteral("timer"), TotpGlyphSize, tint));
        }

        if (!layout.modified.isEmpty()) {
            painter->setFont(metaFont);
            painter->setPen(secondary);
            painter->drawText(
                layout.modified,
                Qt::AlignRight | Qt::AlignVCenter,
                metaMetrics.elidedText(index.data(ModifiedRole).toString(), Qt::ElideRight, layout.modified.width()));
        }

        const QColor menuTint = selected ? content : theme()->color(Role::OnSurfaceVariant);
        painter->setOpacity(MenuOpacity);
        paintGlyph(painter, layout.menu, Icons::pixmap(QStringLiteral("more_vert"), MenuGlyphSize, menuTint));
        painter->setOpacity(1.0);

        painter->restore();
    }

    QSize EntryDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return {MinimumRowWidth, theme()->rowHeight()};
    }

    bool EntryDelegate::editorEvent(QEvent* event,
                                    QAbstractItemModel* model,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index)
    {
        if (event->type() == QEvent::MouseButtonRelease && index.isValid()) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QPoint pos = mouseEvent->position().toPoint();
                if (menuButtonRect(option.rect).contains(pos)) {
                    emit menuRequested(index, mouseEvent->globalPosition().toPoint());
                    return true;
                }
                if (index.data(TotpRole).toBool() && totpButtonRect(option.rect).contains(pos)) {
                    emit totpRequested(index);
                    return true;
                }
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    QRect EntryDelegate::menuButtonRect(const QRect& rowRect) const
    {
        return layoutRow(rowRect.adjusted(0, RowInset, 0, -RowInset), m_compactColumns).menu;
    }

    QRect EntryDelegate::totpButtonRect(const QRect& rowRect) const
    {
        // Geometry only; whether the entry actually has a TOTP seed is a model question.
        return layoutRow(rowRect.adjusted(0, RowInset, 0, -RowInset), m_compactColumns).totp;
    }

    void EntryDelegate::setCompactColumns(bool compact)
    {
        m_compactColumns = compact;
    }

    bool EntryDelegate::compactColumns() const
    {
        return m_compactColumns;
    }

} // namespace Material
