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

#ifndef KEEPASSXC_MATERIALREGEXLAB_H
#define KEEPASSXC_MATERIALREGEXLAB_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace Material
{
    /**
     * The logic behind the regex workbench, kept free of widgets so a test can
     * pin every rule: the token model the Explain tab and the token blocks are
     * built from, the dialect descriptions, the export translations, the
     * replacement translation, the preset library and the cheat sheet. It
     * mirrors design/lib/regex-lab.js, which the reference renders from.
     *
     * Two dialects are described and one is executed: every search bar and
     * this builder run QRegularExpression (PCRE2). Where ECMAScript RegExp
     * disagrees the difference is named rather than smoothed over.
     */
    namespace RegexLab
    {
        struct Token
        {
            /** literal, class, anchor, charclass, group, look, alt, dot, quant, backref, unicode, escape */
            QString type;
            QString text;
            QString english;
            QString cantonese;
            int start = 0;
            int end = 0;
            /** Capture group number, when the token opens one. */
            int group = 0;
            /** Group name, when the token opens a named group. */
            QString name;
            /** True for constructs PCRE2 has and ECMAScript lacks. */
            bool pcreOnly = false;
        };

        /** A flat token list with source spans; enough for explaining and reordering, not a full parse. */
        QVector<Token> tokenize(const QString& pattern);

        struct DialectFlag
        {
            QString flag;
            QString english;
            QString cantonese;
        };

        struct Dialect
        {
            QString id;
            QString label;
            QString cantonese;
            QVector<DialectFlag> flags;
            QStringList notes;
        };

        /** The two dialects the workbench describes: "js" and "qt". */
        QVector<Dialect> dialects();

        struct Preset
        {
            QString id;
            QString name;
            QString cantonese;
            QString pattern;
            QString flags;
            QString sample;
        };

        /** The pattern library, in the design's order. */
        QVector<Preset> presets();

        struct Export
        {
            QString id;
            QString label;
            QString code;
        };

        /** The pattern and flags written for Qt, JavaScript, Python and grep. */
        QVector<Export> translate(const QString& pattern, const QString& flags);

        /**
         * A JavaScript-style replacement ($1, $<name>, $&) written for
         * QString::replace, which takes \\1 and knows no names or $&.
         * Named references resolve through the pattern's own group order.
         */
        QString qtReplacement(const QString& pattern, const QString& replacement);

        struct CheatEntry
        {
            QString token;
            QString english;
            QString cantonese;
            bool pcreOnly = false;
        };

        /** The cheat sheet: one row per construct the builder knows. */
        QVector<CheatEntry> cheatSheet();

        /** Names of the capture groups in @p pattern, in group order. */
        QStringList groupNames(const QString& pattern);
    } // namespace RegexLab
} // namespace Material

#endif // KEEPASSXC_MATERIALREGEXLAB_H
