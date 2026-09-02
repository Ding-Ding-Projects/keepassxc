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

#ifndef KEEPASSXC_MATERIALCOLORPICKER_H
#define KEEPASSXC_MATERIALCOLORPICKER_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;

namespace Material
{
    class Select;
    class Slider;
    class Switch;

    /**
     * The colour translator: one colour read and written in every notation
     * the picker offers. Pure functions, so the picker, the editor and the
     * tests share exactly one set of conversions.
     */
    namespace ColorText
    {
        /** "#RRGGBB", or "#RRGGBBAA" when the alpha is not opaque. */
        QString hex(const QColor& color);
        QString rgb(const QColor& color); // rgb(255 0 0) / rgba(... / 0.5)
        QString hsl(const QColor& color); // hsl(0 100% 50%)
        QString hsv(const QColor& color); // hsv(0 100% 100%)
        QString hwb(const QColor& color); // hwb(0 0% 0%)
        QString cmyk(const QColor& color); // cmyk(0% 100% 100% 0%)
        QString lab(const QColor& color); // lab(53.2 80.1 67.2)  (CIELAB, D65)
        QString lch(const QColor& color); // lch(53.2 104.6 40)
        QString oklab(const QColor& color); // oklab(0.628 0.225 0.126)
        QString oklch(const QColor& color); // oklch(0.628 0.258 29.2)
        /** The CSS named colour when the value is exactly one, else empty. */
        QString name(const QColor& color);
        /** Every notation above, keyed by its label, in display order. */
        QList<QPair<QString, QString>> all(const QColor& color);
        /**
         * Parse any of the notations above (and CSS names). Returns an invalid
         * colour for text that is none of them.
         */
        QColor parse(const QString& text);
        /** WCAG 2.x contrast ratio between two opaque colours, 1..21. */
        double contrastRatio(const QColor& foreground, const QColor& background);
        /** The sentinel a stored colour field carries for the animated rainbow. */
        QString rainbowSentinel();
        bool isRainbow(const QString& stored);
        /** Rainbow speed level 1..5 to its cycle length in milliseconds. */
        int rainbowCycleMs(int level);
    } // namespace ColorText

    /**
     * The infinite colour picker: a two-dimensional saturation/value field, a
     * hue slider, an alpha slider, a translator that reads and writes the
     * colour in every notation, a contrast readout against a reference, recent
     * colours, and the animated rainbow as one of the choices.
     *
     * Swatches and recents are conveniences layered on the continuous field,
     * never a replacement for it.
     */
    class ColorPicker : public QWidget
    {
        Q_OBJECT

    public:
        explicit ColorPicker(QWidget* parent = nullptr);
        ~ColorPicker() override;

        QColor color() const;
        void setColor(const QColor& color);

        /** The colour the contrast readout compares against. */
        QColor referenceColor() const;
        void setReferenceColor(const QColor& color);

        bool isRainbow() const;
        void setRainbow(bool rainbow);
        int rainbowLevel() const;
        void setRainbowLevel(int level);

        /** The translator's line edits, keyed by notation label. */
        QHash<QString, QLineEdit*> notationEdits() const;
        QLabel* contrastLabel() const;
        Switch* rainbowSwitch() const;
        Slider* rainbowSpeed() const;
        QList<QColor> recentColors() const;
        void addRecentColor(const QColor& color);

    signals:
        void colorChanged(const QColor& color);
        void rainbowChanged(bool rainbow, int level);

    private:
        class Field;
        class HueBar;
        class AlphaBar;
        class RecentRow;

        void buildUi();
        void syncFromColor(bool emitChange);
        void applyNotation(const QString& label);
        void applyTheme();

        QColor m_color = QColor(0, 107, 90);
        QColor m_reference = Qt::white;
        bool m_updating = false;
        bool m_rainbow = false;
        int m_rainbowLevel = 3;
        Field* m_field = nullptr;
        HueBar* m_hue = nullptr;
        AlphaBar* m_alpha = nullptr;
        QLabel* m_swatch = nullptr;
        QLabel* m_contrast = nullptr;
        QLabel* m_nameLabel = nullptr;
        QLabel* m_gamutLabel = nullptr;
        QHash<QString, QLineEdit*> m_edits;
        Switch* m_rainbowSwitch = nullptr;
        Slider* m_rainbowSpeed = nullptr;
        QLabel* m_rainbowCaption = nullptr;
        RecentRow* m_recent = nullptr;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALCOLORPICKER_H
