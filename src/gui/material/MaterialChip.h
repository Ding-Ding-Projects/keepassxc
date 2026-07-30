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

#ifndef KEEPASSXC_MATERIALCHIP_H
#define KEEPASSXC_MATERIALCHIP_H

#include "MaterialTheme.h"

#include <QAbstractButton>
#include <QLabel>

namespace Material
{
    /**
     * The eight control pill styles used by the spec sheets and by any row that
     * needs a compact status readout.
     */
    enum class PillKind
    {
        On, // primary on onPrimary
        Off, // transparent, onSurfaceVariant text, outline border
        Value, // secondaryContainer on onSecondaryContainer
        Action, // transparent, primary text, outline border
        Mono, // surfaceContainer on onSurface, monospace
        Good, // greenContainer on onGreenContainer
        Warn, // amberContainer on onAmberContainer
        Bad // errorContainer on onErrorContainer
    };

    /** Background of a pill; an invalid colour means the pill is transparent. */
    QColor pillContainerColor(PillKind kind);
    /** Text and glyph colour of a pill. */
    QColor pillContentColor(PillKind kind);
    /** Border of a pill; an invalid colour means the pill has no outline. */
    QColor pillBorderColor(PillKind kind);

    /**
     * A Material 3 chip.
     *
     * Assist and Suggestion chips act like buttons, Filter chips are checkable
     * and grow a leading check when checked, Input chips carry a trailing
     * glyph that removes them. Setting the kind adjusts the checkable flag, so
     * call setKind() before wiring up toggled().
     *
     * Chips are pills by default; the tag chips in the group pane use
     * setRadius(Shape::Small) for the 8px corners in the design.
     *
     * Colour comes from the PillKind table rather than from fixed roles, so a
     * chip can wear any of the eight pill styles - including the six borderless
     * ones - in either of its two states.
     */
    class Chip : public QAbstractButton
    {
        Q_OBJECT

    public:
        enum class Kind
        {
            Assist,
            Filter,
            Input,
            Suggestion
        };

        explicit Chip(QWidget* parent = nullptr);
        Chip(const QString& text, QWidget* parent = nullptr);
        Chip(const QString& symbol, const QString& text, Kind kind, QWidget* parent = nullptr);
        ~Chip() override;

        Kind kind() const;
        void setKind(Kind kind);

        /** Leading Material Symbols glyph; empty hides it. */
        QString symbol() const;
        void setSymbol(const QString& symbol);

        /** Trailing glyph, `close` on Input chips; empty hides it. */
        QString trailingSymbol() const;
        void setTrailingSymbol(const QString& symbol);

        /**
         * The pill styles the chip paints in its resting and selected states.
         *
         * The defaults - Off resting, Value selected - are the Material filter
         * chip and what nearly every chip in the design uses. The exception is
         * the search bar's regex toggle, which turns solid primary when it is
         * armed: setPills(PillKind::Off, PillKind::On). Routing through the
         * PILL table this way is what lets a chip drop its border, since six of
         * the eight pill styles have none.
         */
        PillKind restingPill() const;
        PillKind selectedPill() const;
        void setPills(PillKind resting, PillKind selected);

        /**
         * Whether a selected chip grows a leading check glyph and reserves the
         * width for it.
         *
         * True by default, which is the Material filter chip. The design's tag
         * chips in the group pane and its regex toggle signal selection with
         * the container colour alone, so those opt out.
         */
        bool showsCheckWhenSelected() const;
        void setShowsCheckWhenSelected(bool shows);

        int radius() const;
        void setRadius(int radius);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        /** The trailing glyph was clicked; the chip itself did not emit clicked(). */
        void trailingActivated();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        /** Hit rect of the trailing glyph, empty when there is none. */
        QRect trailingRect() const;

    private:
        /** Whether the chip is currently painting its selected state. */
        bool isSelected() const;

        Kind m_kind = Kind::Assist;
        QString m_symbol;
        QString m_trailingSymbol;
        int m_radius = Shape::Full;
        bool m_hovered = false;
        // Off / Value reproduce the Material filter chip exactly, so a chip that
        // says nothing about its pills looks the way it always has.
        PillKind m_restingPill = PillKind::Off;
        PillKind m_selectedPill = PillKind::Value;
        bool m_showsCheck = true;
    };

    /**
     * A non-interactive status pill, used for the right-aligned control column
     * of the spec sheet rows and for inline status readouts elsewhere.
     */
    class PillLabel : public QLabel
    {
        Q_OBJECT

    public:
        static constexpr int PillHeight = 32;

        explicit PillLabel(QWidget* parent = nullptr);
        PillLabel(PillKind kind, const QString& text, QWidget* parent = nullptr);
        ~PillLabel() override;

        PillKind pillKind() const;
        void setPillKind(PillKind kind);

        void setPillText(const QString& text);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        PillKind m_kind = PillKind::Off;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCHIP_H
