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

#ifndef KEEPASSXC_MATERIALREGEXTOKENS_H
#define KEEPASSXC_MATERIALREGEXTOKENS_H

#include <QList>
#include <QString>

namespace Material
{
    /**
     * A flat token view of a pattern, for the builder's visual blocks and its
     * per-token explainer.
     *
     * This is deliberately NOT a parser. It does not build a tree, it does not
     * validate, and it does not decide whether a pattern is correct -
     * QRegularExpression is the only authority on that, and its error string is
     * what the builder shows. All this does is split the source into spans a
     * human can point at, so a block can be dragged, removed, or described in
     * two languages.
     *
     * The consequence worth knowing: reordering blocks can produce a pattern
     * that does not compile. That is expected and is the user's business. The
     * builder shows the compiler's own error and refuses to apply, rather than
     * quietly repairing what was asked for.
     */
    struct RegexToken
    {
        enum class Type
        {
            Literal,
            CharClass,
            Class, // \d \w \s and their negations
            Group,
            Look,
            Quantifier,
            Anchor,
            Alternation,
            BackReference,
            UnicodeProperty,
            Escape,
            Dot
        };

        Type type = Type::Literal;
        QString text; // the exact source span
        int start = 0;
        int end = 0;
        int groupNumber = 0; // capture groups only, 1-based
        QString groupName; // named groups only

        /** English description, e.g. "one or more, greedy". */
        QString describeEn() const;
        /** Cantonese description at the active funny level. Facts identical. */
        QString describeYue() const;
        /** True when this construct exists in PCRE2 but not in ECMAScript. */
        bool isPcreOnly() const;
    };

    /** Split @p pattern into tokens. Never throws; never returns partial text. */
    QList<RegexToken> tokenize(const QString& pattern);

    /** Rebuild a pattern from tokens, e.g. after a drag-reorder. */
    QString detokenize(const QList<RegexToken>& tokens);
} // namespace Material

#endif // KEEPASSXC_MATERIALREGEXTOKENS_H
