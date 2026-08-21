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

#ifndef KEEPASSXC_MATERIALVOICESTRINGS_H
#define KEEPASSXC_MATERIALVOICESTRINGS_H

#include <QString>

namespace Material
{
    namespace Voice
    {
        enum class Lang
        {
            En,
            Yue,
            Both
        };

        /**
         * A message in five voices per language.
         *
         * The one invariant, which every reviewer of this file should check
         * before approving a new entry: THE FIVE LEVELS SAY THE SAME THING.
         * They differ in how it reads, never in what it reports. If level 1
         * names the file, level 5 names the file. If level 1 says the action is
         * irreversible, so does level 5. If level 1 quotes the error, so does
         * level 5.
         *
         * This applies with no exemptions - destructive, financial, security,
         * accessibility and error copy included. A warning nobody can act on is
         * a broken warning, not a funny one.
         *
         * Cantonese copy may be locally natural and funny at every level and
         * must stay respectful: humour never mocks the user, their data loss,
         * their money, or their disability.
         *
         * Placeholders are %1-style and are IDENTICAL across all ten strings of
         * an entry, so a caller cannot pass different arguments per level.
         */
        struct Message
        {
            const char* en[5];
            const char* yue[5];
        };

        /** Resolve @p key at @p level (1-5) in @p lang. */
        QString text(const char* key, int level, Lang lang);

        /**
         * Bilingual pair for a label. In Both mode the caller renders the first
         * prominently and the second as a compact secondary line - never two
         * equal-weight labels, which is what crowds a narrow window.
         */
        QPair<QString, QString> label(const char* key);

        /** Per-token regex descriptions for the builder's explainer. */
        QString regexToken(int tokenType, const QString& text, Lang lang);

        /** Parse / serialise for Config storage. */
        Lang langFromString(const QString& v);
        QString langToString(Lang l);
    } // namespace Voice
} // namespace Material

#endif // KEEPASSXC_MATERIALVOICESTRINGS_H
