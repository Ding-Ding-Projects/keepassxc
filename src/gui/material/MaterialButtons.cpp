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

#include "MaterialButtons.h"
#include "MaterialElevation.h"
#include "MaterialIcons.h"

#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QPainter>

namespace Material
{
    namespace
    {
        constexpr int ContentGap = 8; // leading glyph to label
        constexpr int LabelPadding = 16;
        // The design's FAB: padding:0 22px 0 18px, gap:12px, a 24px glyph.
        constexpr int FabLeadingPadding = 18;
        constexpr int FabPadding = 22;
        constexpr int FabContentGap = 12;
        constexpr int FabSymbolSize = 24;
        constexpr int IconButtonSymbolSize = 20;
        constexpr int BadgeHeight = 16;
        constexpr int BadgeInset = 4;
        constexpr int BadgePadding = 4;

        constexpr qreal HoverAlpha = 0.08;
        constexpr qreal PressedAlpha = 0.12;
        constexpr qreal DisabledOpacity = 0.38;

        /** Blend @p fg over an opaque @p bg at @p alpha. */
        QColor blend(const QColor& fg, const QColor& bg, qreal alpha)
        {
            alpha = qBound(0.0, alpha, 1.0);
            return QColor::fromRgbF(fg.redF() * alpha + bg.redF() * (1.0 - alpha),
                                    fg.greenF() * alpha + bg.greenF() * (1.0 - alpha),
                                    fg.blueF() * alpha + bg.blueF() * (1.0 - alpha));
        }
    } // namespace

    // ----------------------------------------------------------------- ButtonBase

    ButtonBase::ButtonBase(QWidget* parent)
        : ButtonBase(QString(), QString(), parent)
    {
    }

    ButtonBase::ButtonBase(const QString& symbol, const QString& text, QWidget* parent)
        : QPushButton(text, parent)
        , m_symbol(symbol)
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        enforceLabelWidth();

        connect(theme(), &Theme::changed, this, [this] {
            enforceLabelWidth();
            updateGeometry();
            update();
        });
    }

    /**
     * Pins the widget's minimum width to the width its label actually needs.
     *
     * A size hint is only a request: a layout that has already sized a row, or one working to a
     * fixed-width sheet, will hand back less and the label silently elides. An action the user
     * cannot read is a broken action, so the minimum is made explicit rather than advisory.
     */
    void ButtonBase::enforceLabelWidth()
    {
        setMinimumWidth(sizeHint().width());
    }

    ButtonBase::~ButtonBase() = default;

    QString ButtonBase::symbol() const
    {
        return m_symbol;
    }

    void ButtonBase::setSymbol(const QString& symbol)
    {
        if (symbol == m_symbol) {
            return;
        }
        m_symbol = symbol;
        updateGeometry();
        update();
    }

    int ButtonBase::radius() const
    {
        return m_radius;
    }

    void ButtonBase::setRadius(int radius)
    {
        if (radius == m_radius) {
            return;
        }
        m_radius = radius;
        update();
    }

    int ButtonBase::symbolSize() const
    {
        return m_symbolSize;
    }

    void ButtonBase::setSymbolSize(int size)
    {
        if (size == m_symbolSize) {
            return;
        }
        m_symbolSize = qMax(1, size);
        updateGeometry();
        update();
    }

    void ButtonBase::setRoles(Role container, Role content)
    {
        m_containerRole = container;
        m_contentRole = content;
        m_rolesOverridden = true;
        update();
    }

    void ButtonBase::clearRoles()
    {
        m_rolesOverridden = false;
        m_containerRole = Role::Primary;
        m_contentRole = Role::OnPrimary;
        update();
    }

    Role ButtonBase::containerRole() const
    {
        return m_containerRole;
    }

    Role ButtonBase::contentRole() const
    {
        return m_contentRole;
    }

    Role ButtonBase::borderRole() const
    {
        return Role::Outline;
    }

    bool ButtonBase::hasContainer() const
    {
        return true;
    }

    bool ButtonBase::hasBorder() const
    {
        return false;
    }

    int ButtonBase::horizontalPadding() const
    {
        return LabelPadding;
    }

    int ButtonBase::leadingPadding() const
    {
        return horizontalPadding();
    }

    int ButtonBase::trailingPadding() const
    {
        return horizontalPadding();
    }

    int ButtonBase::contentGap() const
    {
        return ContentGap;
    }

    QFont ButtonBase::labelFont() const
    {
        return theme()->font(TypeRole::LabelLarge);
    }

    bool ButtonBase::isHovered() const
    {
        return m_hovered;
    }

    QColor ButtonBase::containerColor() const
    {
        // The state layer is folded into the container so a transparent variant
        // can hand back a translucent tint and still paint with one fill.
        qreal state = 0.0;
        if (isEnabled()) {
            state = isDown() ? PressedAlpha : (m_hovered ? HoverAlpha : 0.0);
        }

        QColor result;
        if (hasContainer()) {
            result = theme()->color(m_rolesOverridden ? m_containerRole : containerRole());
            if (state > 0.0) {
                result = blend(theme()->color(Role::OnSurface), result, state);
            }
        } else if (state > 0.0) {
            result = theme()->color(Role::OnSurface);
            result.setAlphaF(static_cast<float>(state));
        }

        if (result.isValid() && !isEnabled()) {
            result.setAlphaF(static_cast<float>(result.alphaF() * DisabledOpacity));
        }
        return result;
    }

    QColor ButtonBase::contentColor() const
    {
        QColor result = theme()->color(m_rolesOverridden ? m_contentRole : contentRole());
        if (!isEnabled()) {
            result.setAlphaF(static_cast<float>(DisabledOpacity));
        }
        return result;
    }

    QColor ButtonBase::borderColor() const
    {
        if (!hasBorder()) {
            return {};
        }
        QColor result = theme()->color(borderRole());
        if (!isEnabled()) {
            result.setAlphaF(static_cast<float>(DisabledOpacity));
        }
        return result;
    }

    QSize ButtonBase::sizeHint() const
    {
        const QFontMetrics metrics(labelFont());
        const int glyph = m_symbol.isEmpty() ? 0 : m_symbolSize;
        const int label = text().isEmpty() ? 0 : metrics.horizontalAdvance(text());
        const int gap = (glyph > 0 && label > 0) ? contentGap() : 0;
        const int height = qMax(Layout::ButtonHeight, metrics.height() + 12);
        const int width = leadingPadding() + trailingPadding() + glyph + gap + label;
        return {qMax(width, height), height};
    }

    QSize ButtonBase::minimumSizeHint() const
    {
        // Deliberately the full label, not an ellipsis width. Allowing a layout to squeeze a
        // button until paintEvent elides it turns "Keep it professional" into
        // "Keep it profession..." and leaves the user guessing what the button does. A crowded
        // row should make its dialog wider; it should never make an action unreadable.
        return sizeHint();
    }

    void ButtonBase::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintSurface(&painter, rect(), m_radius, containerColor(), borderColor());

        const QFont typeface = labelFont();
        const QFontMetrics metrics(typeface);
        const int glyph = m_symbol.isEmpty() ? 0 : m_symbolSize;
        const int gap = (glyph > 0 && !text().isEmpty()) ? contentGap() : 0;
        // Content is centred in the padded content box rather than in the widget,
        // so a variant with asymmetric padding puts its glyph on the leading edge
        // the design asks for. With equal padding the two are the same sum.
        const int boxLeft = leadingPadding();
        const int boxWidth = qMax(0, width() - leadingPadding() - trailingPadding());
        const int available = qMax(0, boxWidth - glyph - gap);
        // Only elide when the text genuinely does not fit. sizeHint() makes `available` exactly
        // the text width, and asking elidedText() to fit a string into precisely its own advance
        // costs it the last word to rounding - which is how "Keep it professional" arrives on
        // screen as "Keep it profession...".
        QString label;
        if (!text().isEmpty()) {
            label = metrics.horizontalAdvance(text()) <= available
                        ? text()
                        : metrics.elidedText(text(), Qt::ElideRight, available);
        }
        const int labelWidth = label.isEmpty() ? 0 : metrics.horizontalAdvance(label);

        // The glyph is tinted opaque and dimmed with the painter instead, so a
        // disabled button fades the icon exactly as much as its label.
        const QColor content = contentColor();
        QColor tint = content;
        tint.setAlpha(255);

        int x = boxLeft + (boxWidth - glyph - gap - labelWidth) / 2;
        if (glyph > 0) {
            const QRect glyphRect(x, (height() - m_symbolSize) / 2, m_symbolSize, m_symbolSize);
            painter.setOpacity(content.alphaF());
            painter.drawPixmap(glyphRect, Icons::pixmap(m_symbol, m_symbolSize, tint));
            painter.setOpacity(1.0);
            x += glyph + gap;
        }

        if (!label.isEmpty()) {
            painter.setFont(typeface);
            painter.setPen(content);
            painter.drawText(QRect(x, 0, labelWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }

    void ButtonBase::enterEvent(QEnterEvent* event)
    {
        QPushButton::enterEvent(event);
        m_hovered = true;
        update();
    }

    void ButtonBase::leaveEvent(QEvent* event)
    {
        QPushButton::leaveEvent(event);
        m_hovered = false;
        update();
    }

    void ButtonBase::changeEvent(QEvent* event)
    {
        QPushButton::changeEvent(event);
        switch (event->type()) {
        case QEvent::EnabledChange:
            m_hovered = m_hovered && isEnabled();
            update();
            break;
        case QEvent::PaletteChange:
            update();
            break;
        case QEvent::FontChange:
        case QEvent::StyleChange:
            enforceLabelWidth();
            updateGeometry();
            update();
            break;
        default:
            break;
        }
    }

    // --------------------------------------------------------------- FilledButton

    FilledButton::FilledButton(QWidget* parent)
        : ButtonBase(parent)
    {
    }

    FilledButton::FilledButton(const QString& symbol, const QString& text, QWidget* parent)
        : ButtonBase(symbol, text, parent)
    {
    }

    Role FilledButton::containerRole() const
    {
        return Role::Primary;
    }

    Role FilledButton::contentRole() const
    {
        return Role::OnPrimary;
    }

    // ---------------------------------------------------------------- TonalButton

    TonalButton::TonalButton(QWidget* parent)
        : ButtonBase(parent)
    {
    }

    TonalButton::TonalButton(const QString& symbol, const QString& text, QWidget* parent)
        : ButtonBase(symbol, text, parent)
    {
    }

    Role TonalButton::containerRole() const
    {
        return Role::SecondaryContainer;
    }

    Role TonalButton::contentRole() const
    {
        return Role::OnSecondaryContainer;
    }

    // ------------------------------------------------------------- OutlinedButton

    OutlinedButton::OutlinedButton(QWidget* parent)
        : ButtonBase(parent)
    {
    }

    OutlinedButton::OutlinedButton(const QString& symbol, const QString& text, QWidget* parent)
        : ButtonBase(symbol, text, parent)
    {
    }

    Role OutlinedButton::contentRole() const
    {
        return Role::OnSurface;
    }

    Role OutlinedButton::borderRole() const
    {
        return Role::Outline;
    }

    bool OutlinedButton::hasContainer() const
    {
        return false;
    }

    bool OutlinedButton::hasBorder() const
    {
        return true;
    }

    // ----------------------------------------------------------------- TextButton

    TextButton::TextButton(QWidget* parent)
        : ButtonBase(parent)
    {
    }

    TextButton::TextButton(const QString& symbol, const QString& text, QWidget* parent)
        : ButtonBase(symbol, text, parent)
    {
    }

    Role TextButton::contentRole() const
    {
        return Role::Primary;
    }

    bool TextButton::hasContainer() const
    {
        return false;
    }

    bool TextButton::hasBorder() const
    {
        return false;
    }

    // ----------------------------------------------------------------- IconButton

    IconButton::IconButton(QWidget* parent)
        : IconButton(QString(), parent)
    {
    }

    IconButton::IconButton(const QString& symbol, QWidget* parent)
        : ButtonBase(symbol, QString(), parent)
    {
        setSymbolSize(IconButtonSymbolSize);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        // ButtonBase's constructor already ran enforceLabelWidth(), but virtual
        // dispatch could not reach IconButton::sizeHint() yet, so the minimum it
        // pinned came from the label formula - padding plus a glyph, 50px - for a
        // button that has no label. Re-pin it now that sizeHint() answers with the
        // diameter, otherwise that stale 50 outvotes every later setDiameter().
        enforceLabelWidth();
    }

    int IconButton::diameter() const
    {
        return m_diameter;
    }

    void IconButton::setDiameter(int diameter)
    {
        diameter = qMax(1, diameter);
        if (diameter == m_diameter) {
            return;
        }
        m_diameter = diameter;
        // The diameter is the entire geometry of a round button, so it has to be
        // the minimum as well as the hint. Leaving the inherited label-derived
        // minimum in place is what forced callers to follow setDiameter() with a
        // setFixedSize() of the same value.
        enforceLabelWidth();
        updateGeometry();
        update();
    }

    int IconButton::badgeCount() const
    {
        return m_badgeCount;
    }

    void IconButton::setBadgeCount(int count)
    {
        count = qMax(0, count);
        if (count == m_badgeCount) {
            return;
        }
        m_badgeCount = count;
        update();
    }

    bool IconButton::isFilled() const
    {
        return m_filled;
    }

    void IconButton::setFilled(bool filled)
    {
        if (filled == m_filled) {
            return;
        }
        m_filled = filled;
        update();
    }

    QSize IconButton::sizeHint() const
    {
        return {m_diameter, m_diameter};
    }

    QSize IconButton::minimumSizeHint() const
    {
        return {m_diameter, m_diameter};
    }

    Role IconButton::containerRole() const
    {
        return Role::SecondaryContainer;
    }

    Role IconButton::contentRole() const
    {
        return m_filled ? Role::OnSecondaryContainer : Role::OnSurfaceVariant;
    }

    bool IconButton::hasContainer() const
    {
        return m_filled;
    }

    bool IconButton::hasBorder() const
    {
        return false;
    }

    void IconButton::paintEvent(QPaintEvent* event)
    {
        ButtonBase::paintEvent(event);
        if (m_badgeCount <= 0) {
            return;
        }

        const QString label = m_badgeCount > 99 ? QStringLiteral("99+") : QString::number(m_badgeCount);
        const QFont badgeFont = theme()->font(TypeRole::LabelSmall);
        const QFontMetrics metrics(badgeFont);
        const int badgeWidth = qMax(BadgeHeight, metrics.horizontalAdvance(label) + 2 * BadgePadding);
        const QRect badge(width() - badgeWidth - BadgeInset, BadgeInset, badgeWidth, BadgeHeight);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) {
            painter.setOpacity(DisabledOpacity);
        }
        paintSurface(&painter, badge, Shape::Full, theme()->color(Role::Error));
        painter.setFont(badgeFont);
        painter.setPen(theme()->color(Role::OnError));
        painter.drawText(badge, Qt::AlignCenter, label);
    }

    // ---------------------------------------------------------------- ExtendedFab

    ExtendedFab::ExtendedFab(QWidget* parent)
        : ExtendedFab(QString(), QString(), parent)
    {
    }

    ExtendedFab::ExtendedFab(const QString& symbol, const QString& text, QWidget* parent)
        : ButtonBase(symbol, text, parent)
    {
        setRadius(Shape::Rail);
        setSymbolSize(FabSymbolSize);
        // The shadow lives outside the widget rect, so it has to be an effect
        // rather than something paintEvent() draws.
        setGraphicsEffect(elevation(3, this));
    }

    QSize ExtendedFab::sizeHint() const
    {
        QSize hint = ButtonBase::sizeHint();
        hint.setHeight(Layout::FabHeight);
        return hint;
    }

    Role ExtendedFab::containerRole() const
    {
        return Role::PrimaryContainer;
    }

    Role ExtendedFab::contentRole() const
    {
        return Role::OnPrimaryContainer;
    }

    int ExtendedFab::horizontalPadding() const
    {
        return FabPadding;
    }

    int ExtendedFab::leadingPadding() const
    {
        // Only the leading edge differs; trailingPadding() keeps answering
        // horizontalPadding(), which is already the design's 22px.
        return FabLeadingPadding;
    }

    int ExtendedFab::contentGap() const
    {
        return FabContentGap;
    }

    QFont ExtendedFab::labelFont() const
    {
        // The design's FAB label is 15px at weight 500 - BodyLarge's size carrying
        // LabelLarge's weight - which no single type role expresses on its own.
        QFont typeface = theme()->font(TypeRole::BodyLarge);
        typeface.setWeight(QFont::Medium);
        return typeface;
    }

} // namespace Material
