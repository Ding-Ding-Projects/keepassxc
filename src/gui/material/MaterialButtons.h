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

#ifndef KEEPASSXC_MATERIALBUTTONS_H
#define KEEPASSXC_MATERIALBUTTONS_H

#include "MaterialTheme.h"

#include <QPushButton>

namespace Material
{
    /**
     * Shared behaviour for every Material button.
     *
     * Carries a Material Symbols name that is re-tinted with the resolved
     * content colour, a shape radius and the hover / pressed / disabled state
     * layers. Variants differ only in the colour roles they answer with, so a
     * one-off tint - the destructive delete button, a rail toggle - is a
     * setRoles() call rather than a new class.
     */
    class ButtonBase : public QPushButton
    {
        Q_OBJECT

    public:
        explicit ButtonBase(QWidget* parent = nullptr);
        ButtonBase(const QString& symbol, const QString& text, QWidget* parent = nullptr);
        ~ButtonBase() override;

        QString symbol() const;
        void setSymbol(const QString& symbol);

        int radius() const;
        void setRadius(int radius);

        /** Size of the leading glyph in logical pixels; defaults to 18. */
        int symbolSize() const;
        void setSymbolSize(int size);

        /** Replace the variant's colour roles, e.g. Error / OnError for a destructive action. */
        void setRoles(Role container, Role content);
        void clearRoles();

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void changeEvent(QEvent* event) override;

        /** Variant colour roles. Honoured only when setRoles() is not in effect. */
        virtual Role containerRole() const;
        virtual Role contentRole() const;
        virtual Role borderRole() const;

        /** Whether the variant paints a filled container and a hairline border. */
        virtual bool hasContainer() const;
        virtual bool hasBorder() const;

        /** Padding either side of the content; the FAB and icon buttons widen it. */
        virtual int horizontalPadding() const;

        /**
         * The leading and trailing padding taken separately.
         *
         * Both answer horizontalPadding() unless a variant says otherwise. The
         * FAB is the one control the design gives asymmetric padding to, because
         * its 24px glyph reads optically wider than the label's right sidebearing.
         */
        virtual int leadingPadding() const;
        virtual int trailingPadding() const;

        /** Gap between the leading glyph and the label. */
        virtual int contentGap() const;

        /** Typeface of the label; the FAB steps up a size. */
        virtual QFont labelFont() const;

        /** Resolved colours, including the hover, pressed and disabled treatments. */
        QColor containerColor() const;
        QColor contentColor() const;
        QColor borderColor() const;

        bool isHovered() const;

        /** Pins the minimum width to what the label needs, so no action can elide. */
        void enforceLabelWidth();

    private:
        QString m_symbol;
        int m_radius = Shape::Full;
        int m_symbolSize = 18;
        bool m_hovered = false;
        bool m_rolesOverridden = false;
        Role m_containerRole = Role::Primary;
        Role m_contentRole = Role::OnPrimary;
    };

    /** The high emphasis action: primary container, onPrimary content. */
    class FilledButton : public ButtonBase
    {
        Q_OBJECT

    public:
        explicit FilledButton(QWidget* parent = nullptr);
        FilledButton(const QString& symbol, const QString& text, QWidget* parent = nullptr);

    protected:
        Role containerRole() const override;
        Role contentRole() const override;
    };

    /** Secondary emphasis: secondaryContainer, onSecondaryContainer content. */
    class TonalButton : public ButtonBase
    {
        Q_OBJECT

    public:
        explicit TonalButton(QWidget* parent = nullptr);
        TonalButton(const QString& symbol, const QString& text, QWidget* parent = nullptr);

    protected:
        Role containerRole() const override;
        Role contentRole() const override;
    };

    /** Transparent with an outline border and primary content. */
    class OutlinedButton : public ButtonBase
    {
        Q_OBJECT

    public:
        explicit OutlinedButton(QWidget* parent = nullptr);
        OutlinedButton(const QString& symbol, const QString& text, QWidget* parent = nullptr);

    protected:
        Role contentRole() const override;
        Role borderRole() const override;
        bool hasContainer() const override;
        bool hasBorder() const override;
    };

    /** Transparent, no border, primary content. The lowest emphasis action. */
    class TextButton : public ButtonBase
    {
        Q_OBJECT

    public:
        explicit TextButton(QWidget* parent = nullptr);
        TextButton(const QString& symbol, const QString& text, QWidget* parent = nullptr);

    protected:
        Role contentRole() const override;
        bool hasContainer() const override;
        bool hasBorder() const override;
    };

    /**
     * A round 40px glyph button. Used across the app bar, the tab strip, the
     * rail footer and every card header. Optionally carries an error-coloured
     * count badge over its top right corner.
     */
    class IconButton : public ButtonBase
    {
        Q_OBJECT

    public:
        explicit IconButton(QWidget* parent = nullptr);
        explicit IconButton(const QString& symbol, QWidget* parent = nullptr);

        /** Diameter of the round hit target; defaults to Layout::IconButtonSize. */
        int diameter() const;
        void setDiameter(int diameter);

        /** Count drawn over the top right corner; zero hides the badge. */
        int badgeCount() const;
        void setBadgeCount(int count);

        /** Paint a filled container behind the glyph instead of leaving it transparent. */
        bool isFilled() const;
        void setFilled(bool filled);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

        Role containerRole() const override;
        Role contentRole() const override;
        bool hasContainer() const override;
        bool hasBorder() const override;

    private:
        int m_diameter = Layout::IconButtonSize;
        int m_badgeCount = 0;
        bool m_filled = false;
    };

    /**
     * The 56px extended FAB pinned to the bottom right of the entry list.
     * primaryContainer on onPrimaryContainer, an 18px radius and an el3 shadow.
     *
     * Its metrics are its own: 18px leading / 22px trailing padding, a 12px gap
     * to the label and a 15px medium label, all larger than the shared button
     * scale because the FAB is the screen's single most prominent action.
     */
    class ExtendedFab : public ButtonBase
    {
        Q_OBJECT

    public:
        explicit ExtendedFab(QWidget* parent = nullptr);
        ExtendedFab(const QString& symbol, const QString& text, QWidget* parent = nullptr);

        QSize sizeHint() const override;

    protected:
        Role containerRole() const override;
        Role contentRole() const override;
        int horizontalPadding() const override;
        int leadingPadding() const override;
        int contentGap() const override;
        QFont labelFont() const override;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALBUTTONS_H
