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

#include "MaterialChip.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

namespace Material
{
    namespace
    {
        constexpr int ChipPadding = 12;
        constexpr int ChipGap = 6;
        constexpr int ChipSymbolSize = 18;

        constexpr int PillPadding = 14;
        constexpr int PillMaxWidth = 320;

        constexpr qreal HoverAlpha = 0.08;
        constexpr qreal PressedAlpha = 0.12;
        constexpr qreal DisabledOpacity = 0.38;

        /** The 12px medium label the pills use, in mono for the Mono kind. */
        QFont pillFont(PillKind kind)
        {
            QFont font = theme()->font(TypeRole::LabelMedium);
            if (kind == PillKind::Mono) {
                font.setFamily(Theme::monoFamily());
            }
            return font;
        }
    } // namespace

    QColor pillContainerColor(PillKind kind)
    {
        switch (kind) {
        case PillKind::On:
            return theme()->color(Role::Primary);
        case PillKind::Value:
            return theme()->color(Role::SecondaryContainer);
        case PillKind::Mono:
            return theme()->color(Role::SurfaceContainer);
        case PillKind::Good:
            return theme()->color(Role::GreenContainer);
        case PillKind::Warn:
            return theme()->color(Role::AmberContainer);
        case PillKind::Bad:
            return theme()->color(Role::ErrorContainer);
        case PillKind::Off:
        case PillKind::Action:
            break;
        }
        return {};
    }

    QColor pillContentColor(PillKind kind)
    {
        switch (kind) {
        case PillKind::On:
            return theme()->color(Role::OnPrimary);
        case PillKind::Off:
            return theme()->color(Role::OnSurfaceVariant);
        case PillKind::Value:
            return theme()->color(Role::OnSecondaryContainer);
        case PillKind::Action:
            return theme()->color(Role::Primary);
        case PillKind::Mono:
            return theme()->color(Role::OnSurface);
        case PillKind::Good:
            return theme()->color(Role::OnGreenContainer);
        case PillKind::Warn:
            return theme()->color(Role::OnAmberContainer);
        case PillKind::Bad:
            return theme()->color(Role::OnErrorContainer);
        }
        return theme()->color(Role::OnSurface);
    }

    QColor pillBorderColor(PillKind kind)
    {
        if (kind == PillKind::Off || kind == PillKind::Action) {
            return theme()->color(Role::Outline);
        }
        return {};
    }

    // ------------------------------------------------------------------ Chip

    Chip::Chip(QWidget* parent)
        : Chip(QString(), QString(), Kind::Assist, parent)
    {
    }

    Chip::Chip(const QString& text, QWidget* parent)
        : Chip(QString(), text, Kind::Assist, parent)
    {
    }

    Chip::Chip(const QString& symbol, const QString& text, Kind kind, QWidget* parent)
        : QAbstractButton(parent)
        , m_symbol(symbol)
    {
        setText(text);
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        setKind(kind);

        // A filter chip grows a leading check, so its geometry changes with state.
        connect(this, &QAbstractButton::toggled, this, [this] {
            updateGeometry();
            update();
        });
        connect(theme(), &Theme::changed, this, [this] {
            updateGeometry();
            update();
        });
    }

    Chip::~Chip() = default;

    Chip::Kind Chip::kind() const
    {
        return m_kind;
    }

    void Chip::setKind(Kind kind)
    {
        m_kind = kind;
        setCheckable(kind == Kind::Filter);
        if (kind == Kind::Input && m_trailingSymbol.isEmpty()) {
            m_trailingSymbol = QStringLiteral("close");
        }
        updateGeometry();
        update();
    }

    QString Chip::symbol() const
    {
        return m_symbol;
    }

    void Chip::setSymbol(const QString& symbol)
    {
        if (symbol == m_symbol) {
            return;
        }
        m_symbol = symbol;
        updateGeometry();
        update();
    }

    QString Chip::trailingSymbol() const
    {
        return m_trailingSymbol;
    }

    void Chip::setTrailingSymbol(const QString& symbol)
    {
        if (symbol == m_trailingSymbol) {
            return;
        }
        m_trailingSymbol = symbol;
        updateGeometry();
        update();
    }

    PillKind Chip::restingPill() const
    {
        return m_restingPill;
    }

    PillKind Chip::selectedPill() const
    {
        return m_selectedPill;
    }

    void Chip::setPills(PillKind resting, PillKind selected)
    {
        if (resting == m_restingPill && selected == m_selectedPill) {
            return;
        }
        m_restingPill = resting;
        m_selectedPill = selected;
        update();
    }

    bool Chip::showsCheckWhenSelected() const
    {
        return m_showsCheck;
    }

    void Chip::setShowsCheckWhenSelected(bool shows)
    {
        if (shows == m_showsCheck) {
            return;
        }
        m_showsCheck = shows;
        // The check is part of the chip's width, so dropping it resizes the chip.
        updateGeometry();
        update();
    }

    bool Chip::isSelected() const
    {
        return isCheckable() && isChecked();
    }

    int Chip::radius() const
    {
        return m_radius;
    }

    void Chip::setRadius(int radius)
    {
        if (radius == m_radius) {
            return;
        }
        m_radius = radius;
        update();
    }

    QRect Chip::trailingRect() const
    {
        if (m_trailingSymbol.isEmpty()) {
            return {};
        }
        return {
            width() - ChipPadding - ChipSymbolSize, (height() - ChipSymbolSize) / 2, ChipSymbolSize, ChipSymbolSize};
    }

    QSize Chip::sizeHint() const
    {
        const QFontMetrics metrics(theme()->font(TypeRole::BodySmall));
        // Same predicate paintEvent uses to pick its leading glyph, so the width
        // reserved here and the width drawn there can never drift apart.
        const bool leading = !m_symbol.isEmpty() || (isSelected() && m_showsCheck);
        const int label = text().isEmpty() ? 0 : metrics.horizontalAdvance(text());

        int width = 2 * ChipPadding + label;
        if (leading) {
            width += ChipSymbolSize + (label > 0 ? ChipGap : 0);
        }
        if (!m_trailingSymbol.isEmpty()) {
            width += ChipSymbolSize + ChipGap;
        }
        return {qMax(width, Layout::ChipHeight), Layout::ChipHeight};
    }

    QSize Chip::minimumSizeHint() const
    {
        const QFontMetrics metrics(theme()->font(TypeRole::BodySmall));
        const bool leading = !m_symbol.isEmpty() || (isSelected() && m_showsCheck);
        const int label = text().isEmpty() ? 0 : metrics.horizontalAdvance(QStringLiteral("..."));

        int width = 2 * ChipPadding + label;
        if (leading) {
            width += ChipSymbolSize + (label > 0 ? ChipGap : 0);
        }
        if (!m_trailingSymbol.isEmpty()) {
            width += ChipSymbolSize + ChipGap;
        }
        return {qMax(width, Layout::ChipHeight), Layout::ChipHeight};
    }

    void Chip::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        // Both states resolve through the PILL table, which is where the design
        // decides whether a variant is filled, outlined or neither. Six of the
        // eight pills have no border at all, so this is also how a chip goes
        // borderless.
        const bool selected = isSelected();
        const PillKind pill = selected ? m_selectedPill : m_restingPill;
        const QColor fill = pillContainerColor(pill);
        const QColor border = pillBorderColor(pill);
        const QColor content = pillContentColor(pill);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintSurface(&painter, rect(), m_radius, fill, border);

        if (isEnabled() && (isDown() || m_hovered)) {
            paintStateLayer(
                &painter, rect(), m_radius, theme()->color(Role::OnSurface), isDown() ? PressedAlpha : HoverAlpha);
        }

        if (!isEnabled()) {
            painter.setOpacity(DisabledOpacity);
        }

        const QString leadingSymbol =
            !m_symbol.isEmpty() ? m_symbol : (selected && m_showsCheck ? QStringLiteral("check") : QString());
        const QRect trailing = trailingRect();
        const QFont labelFont = theme()->font(TypeRole::BodySmall);
        const QFontMetrics metrics(labelFont);

        int left = ChipPadding;
        if (!leadingSymbol.isEmpty()) {
            const QRect glyph(left, (height() - ChipSymbolSize) / 2, ChipSymbolSize, ChipSymbolSize);
            painter.drawPixmap(glyph, Icons::pixmap(leadingSymbol, ChipSymbolSize, content));
            left += ChipSymbolSize + ChipGap;
        }

        const int right = trailing.isNull() ? width() - ChipPadding : trailing.left() - ChipGap;
        if (!trailing.isNull()) {
            painter.drawPixmap(trailing, Icons::pixmap(m_trailingSymbol, ChipSymbolSize, content));
        }

        if (!text().isEmpty()) {
            const QRect labelRect(left, 0, qMax(0, right - left), height());
            painter.setFont(labelFont);
            painter.setPen(content);
            painter.drawText(labelRect,
                             Qt::AlignLeft | Qt::AlignVCenter,
                             metrics.elidedText(text(), Qt::ElideRight, labelRect.width()));
        }
    }

    void Chip::enterEvent(QEnterEvent* event)
    {
        QAbstractButton::enterEvent(event);
        m_hovered = true;
        update();
    }

    void Chip::leaveEvent(QEvent* event)
    {
        QAbstractButton::leaveEvent(event);
        m_hovered = false;
        update();
    }

    void Chip::mouseReleaseEvent(QMouseEvent* event)
    {
        const QRect trailing = trailingRect();
        if (event->button() == Qt::LeftButton && !trailing.isNull() && trailing.contains(event->position().toPoint())) {
            setDown(false);
            update();
            event->accept();
            emit trailingActivated();
            return;
        }
        QAbstractButton::mouseReleaseEvent(event);
    }

    // ------------------------------------------------------------- PillLabel

    PillLabel::PillLabel(QWidget* parent)
        : PillLabel(PillKind::Off, QString(), parent)
    {
    }

    PillLabel::PillLabel(PillKind kind, const QString& text, QWidget* parent)
        : QLabel(text, parent)
        , m_kind(kind)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        connect(theme(), &Theme::changed, this, [this] {
            updateGeometry();
            update();
        });
    }

    PillLabel::~PillLabel() = default;

    PillKind PillLabel::pillKind() const
    {
        return m_kind;
    }

    void PillLabel::setPillKind(PillKind kind)
    {
        if (kind == m_kind) {
            return;
        }
        m_kind = kind;
        updateGeometry();
        update();
    }

    void PillLabel::setPillText(const QString& text)
    {
        if (text == QLabel::text()) {
            return;
        }
        QLabel::setText(text);
        updateGeometry();
        update();
    }

    QSize PillLabel::sizeHint() const
    {
        const QFontMetrics metrics(pillFont(m_kind));
        const int width = 2 * PillPadding + metrics.horizontalAdvance(text());
        return {qMin(width, PillMaxWidth), PillHeight};
    }

    QSize PillLabel::minimumSizeHint() const
    {
        const QFontMetrics metrics(pillFont(m_kind));
        const int width = 2 * PillPadding + metrics.horizontalAdvance(QStringLiteral("..."));
        return {width, PillHeight};
    }

    void PillLabel::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        // Qualified: QFrame, which QLabel derives from, also has a Shape enum.
        paintSurface(&painter, rect(), Material::Shape::Small, pillContainerColor(m_kind), pillBorderColor(m_kind));

        const QFont font = pillFont(m_kind);
        const QFontMetrics metrics(font);
        const QRect textRect = rect().adjusted(PillPadding, 0, -PillPadding, 0);
        painter.setFont(font);
        painter.setPen(pillContentColor(m_kind));
        painter.drawText(textRect, Qt::AlignCenter, metrics.elidedText(text(), Qt::ElideRight, textRect.width()));
    }

} // namespace Material
