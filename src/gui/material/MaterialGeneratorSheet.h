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

#ifndef KEEPASSXC_MATERIALGENERATORSHEET_H
#define KEEPASSXC_MATERIALGENERATORSHEET_H

#include "MaterialOverlay.h"

#include <QHash>
#include <QString>

class QLabel;
class QSlider;

namespace Material
{
    class ButtonBase;
    /** The entropy bar and the charset pill, defined in MaterialGeneratorSheet.cpp. */
    class EntropyMeter;
    class CharsetPill;

    /**
     * The password generator overlay.
     *
     * A 560px sheet: the generated value in monospace, an entropy meter, the
     * length slider, the character class pills and a Regenerate / Copy action
     * row. Values are drawn from QRandomGenerator::system() over the selected
     * pools and the meter reports length * log2(pool size).
     *
     * This is the presentation layer only - it is deliberately not KeePassXC's
     * PasswordGenerator. The sheet never touches the clipboard itself: it
     * reports the value through passwordCopied() and the host decides where
     * the password goes.
     */
    class GeneratorSheet : public Overlay
    {
        Q_OBJECT

    public:
        /** The character pools the filter chips switch on and off. */
        enum class CharClass
        {
            Upper,
            Lower,
            Digits,
            Special,
            Extended
        };

        explicit GeneratorSheet(QWidget* parent = nullptr);
        ~GeneratorSheet() override;

        QString password() const;

        int length() const;
        void setLength(int length);

        bool isClassEnabled(CharClass charClass) const;
        void setClassEnabled(CharClass charClass, bool enabled);

        /** length * log2(pool size), the figure the meter shows. */
        double entropyBits() const;

    public slots:
        /** Draw a fresh password from the selected pools. */
        void regenerate();

    signals:
        /** The Copy action was pressed. The host owns the clipboard. */
        void passwordCopied(const QString& password);

    protected:
        /** Every opening starts from a fresh value. */
        void aboutToOpen() override;

    private:
        QWidget* buildHeader();
        QWidget* buildValueBox();
        QWidget* buildMeter();
        QWidget* buildLength();
        QWidget* buildCharsets();
        QWidget* buildFooter();
        QString characterPool() const;
        void updateReadouts();
        void applyTheme();

        QWidget* m_sheet = nullptr;
        QLabel* m_valueLabel = nullptr;
        EntropyMeter* m_meter = nullptr;
        QLabel* m_entropyLabel = nullptr;
        QLabel* m_lengthValue = nullptr;
        QSlider* m_lengthSlider = nullptr;
        ButtonBase* m_copyButton = nullptr;
        QHash<int, CharsetPill*> m_charsetPills;
        QString m_password;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALGENERATORSHEET_H
