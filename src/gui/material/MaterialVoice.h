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

#ifndef KEEPASSXC_MATERIALVOICE_H
#define KEEPASSXC_MATERIALVOICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QWidget;

namespace Material
{
    /**
     * The voice of every user-facing message.
     *
     * Copy is looked up by string id in a resource-backed catalogue that holds
     * one line per language per playfulness level. The level styles the wording
     * of every category without exception - destructive, security and error copy
     * included - but it may never change the facts. Each catalogue entry
     * therefore declares the substrings that carry its facts, and a variant that
     * drops one of them is discarded at resolve time in favour of a plainer
     * level. Level 1 is the contract: it is the existing KeePassXC wording and
     * the floor every fallback chain ends at.
     */
    namespace Voice
    {
        enum class Language
        {
            English,
            Cantonese,
            Bilingual
        };

        /**
         * The caller's classification of a message. It travels with the resolved
         * line so a host can tint a snackbar or pick a dialog glyph. It never
         * exempts a message from the playfulness level.
         */
        enum class Category
        {
            Info,
            Success,
            Warning,
            Error,
            Destructive,
            Security
        };

        constexpr int MinLevel = 1;
        constexpr int MaxLevel = 5;
        constexpr int DefaultLevel = 3;

        /** Separates the two halves of a bilingual say() result. */
        constexpr char16_t BilingualSeparator = u'\n';

        /**
         * A resolved message. In bilingual mode @a primary is the English line
         * and @a secondary the Cantonese one, so a host can render the first
         * prominently and the second compactly; otherwise @a secondary is empty.
         */
        struct Line
        {
            QString primary;
            QString secondary;
            Category category = Category::Info;

            bool hasSecondary() const
            {
                return !secondary.isEmpty();
            }

            /** Both halves in one string, separated by BilingualSeparator. */
            QString joined() const;
        };

        QString say(const QString& key, Category category = Category::Info);
        QString say(const QString& key, const QVariantMap& args, Category category);

        /** The structured form of say(), for hosts that lay out the two halves. */
        Line line(const QString& key, const QVariantMap& args = {}, Category category = Category::Info);

        /**
         * Resolve against explicit settings instead of the stored ones, so a
         * settings preview can follow a slider that has not been committed yet.
         */
        Line preview(Language language,
                     int englishLevel,
                     int cantoneseLevel,
                     const QString& key,
                     const QVariantMap& args = {},
                     Category category = Category::Info);

        Language language();
        void setLanguage(Language language);

        /**
         * The playfulness level of a language, 1 (fully professional) to 5.
         * Bilingual has no slider of its own and answers with the English one.
         */
        int funnyLevel(Language language);
        void setFunnyLevel(Language language, int level);

        /** English, level 3 in both languages. */
        void resetToDefaults();

        Language languageFromString(const QString& value);
        QString languageToString(Language language);
        QString languageDisplayName(Language language);
        /** The name of a level, from "Professional" to "Maximum playfulness". */
        QString levelName(int level);
        QString categoryToString(Category category);

        /** Every id in the catalogue, sorted. */
        QStringList catalogueKeys();
        /** The substrings every variant of @p key must keep in @p language. */
        QStringList facts(const QString& key, Language language);

        /** The plain statement shown at first run and beneath the sliders. */
        QString disclosureText();
        bool disclosurePending();
        /** Open the first-run disclosure sheet over @p parent and record it. */
        void presentDisclosure(QWidget* parent);

        /** Emits changed() whenever the language or either level moves. */
        class Notifier : public QObject
        {
            Q_OBJECT

        public:
            static Notifier* instance();

            /** Announce that the stored voice settings changed. */
            void announce();

        signals:
            void changed();

        private:
            Notifier();
        };

        inline Notifier* notifier()
        {
            return Notifier::instance();
        }

    } // namespace Voice

} // namespace Material

#endif // KEEPASSXC_MATERIALVOICE_H
