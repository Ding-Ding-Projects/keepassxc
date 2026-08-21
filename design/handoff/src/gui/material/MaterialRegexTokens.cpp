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

#include "MaterialRegexTokens.h"

#include "MaterialVoiceStrings.h"

namespace Material
{
    // Reference implementation: utils/design/regex-lab.js tokenize(). Port it
    // rather than re-deriving it - the prototype's token boundaries are what
    // the explainer copy was written against, and the two must not drift.
    QList<RegexToken> tokenize(const QString& pattern)
    {
        QList<RegexToken> out;
        int i = 0;
        int groupNo = 0;
        Q_UNUSED(groupNo)
        while (i < pattern.size()) {
            // TODO: port the eleven cases in order:
            //   backslash escapes (\p{...}, \k<name>, \1..\9, class shorthands)
            //   character class [...] with nested ] handling
            //   groups ( (?: (?= (?! (?<= (?<! (?> (?<name>
            //   ) | ^ $ .
            //   quantifiers * + ? {n,m} with lazy ? and possessive + suffixes
            //   literal runs
            ++i;
        }
        return out;
    }

    QString detokenize(const QList<RegexToken>& tokens)
    {
        QString out;
        for (const auto& t : tokens) {
            out += t.text;
        }
        return out;
    }

    QString RegexToken::describeEn() const
    {
        // TODO: VoiceStrings::regexToken(type, text, Voice::Lang::En).
        return {};
    }

    QString RegexToken::describeYue() const
    {
        // TODO: same table, Voice::Lang::Yue. The funny level styles the
        // wording; the construct named never changes.
        return {};
    }

    bool RegexToken::isPcreOnly() const
    {
        // Atomic groups, possessive quantifiers, recursion, \A \z \G.
        return text.startsWith(QLatin1String("(?>")) || text.endsWith(QLatin1Char('+'))
               || text == QLatin1String("(?R)");
    }
} // namespace Material
