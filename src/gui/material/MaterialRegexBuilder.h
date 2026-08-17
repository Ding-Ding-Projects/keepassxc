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

#ifndef KEEPASSXC_MATERIALREGEXBUILDER_H
#define KEEPASSXC_MATERIALREGEXBUILDER_H

#include "MaterialOverlay.h"

#include <QHash>
#include <QString>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QVBoxLayout;

namespace Material
{
    class ButtonBase;
    /** Implementation details of the builder, defined in MaterialRegexBuilder.cpp. */
    class RegexPanel;
    class RegexTokenChip;

    /**
     * The regex builder overlay.
     *
     * A 1000px sheet with the token palette on the left and the pattern
     * workbench on the right: the `/pattern/` field with its flag chips, a
     * status line, a sample text area and the list of matches with their
     * capture groups. Everything is recomputed with QRegularExpression on every
     * keystroke, so the sheet is a live preview of what the search bar will do.
     *
     * The builder never touches the clipboard or the search itself: Copy and
     * Apply report the pattern through patternCopied() and patternApplied(),
     * and the host decides what to do with it.
     */
    class RegexBuilder : public Overlay
    {
        Q_OBJECT

    public:
        explicit RegexBuilder(QWidget* parent = nullptr);
        ~RegexBuilder() override;

        QString pattern() const;
        void setPattern(const QString& pattern);

        /** The sample the matches are computed against. */
        QString sampleText() const;
        void setSampleText(const QString& text);

        /** The active flag chips, a subset of "gimsu" in that order. */
        QString flags() const;
        void setFlags(const QString& flags);

        /** Whether the current pattern compiles. */
        bool isValid() const;

    signals:
        /** The Apply button was pressed; the sheet has closed itself. */
        void patternApplied(const QString& pattern);
        /** The Copy button was pressed. The host owns the clipboard. */
        void patternCopied(const QString& pattern);

    protected:
        void aboutToOpen() override;

    private:
        QWidget* buildHeader();
        QWidget* buildPalette();
        QWidget* buildEditor();
        QWidget* buildFooter();
        void addFlagChip(QHBoxLayout* row, const QString& flag, const QString& hint);
        void insertToken(const QString& token, int caretBack);
        void evaluate();
        void applyTheme();

        QWidget* m_sheet = nullptr;
        RegexPanel* m_patternBox = nullptr;
        QLineEdit* m_patternEdit = nullptr;
        QLabel* m_statusLabel = nullptr;
        QPlainTextEdit* m_sampleEdit = nullptr;
        RegexPanel* m_matchPanel = nullptr;
        QVBoxLayout* m_matchLayout = nullptr;
        ButtonBase* m_copyButton = nullptr;
        ButtonBase* m_applyButton = nullptr;
        QHash<QString, RegexTokenChip*> m_flagChips;
        bool m_valid = true;
    };

} // namespace Material

#endif // KEEPASSXC_MATERIALREGEXBUILDER_H
