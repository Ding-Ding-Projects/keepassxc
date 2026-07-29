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

#include "MaterialVoice.h"

#include "MaterialButtons.h"
#include "MaterialDialog.h"

#include "core/Config.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

// Q_INIT_RESOURCE declares its helper in the enclosing namespace, so the
// catalogue resource has to be registered from file scope.
static void initVoiceCatalogue()
{
    Q_INIT_RESOURCE(voice);
}

namespace Material
{
    namespace Voice
    {
        namespace
        {
            /** One language of one catalogue entry. */
            struct Localised
            {
                QMap<int, QString> levels;
                QStringList facts;
            };

            struct Entry
            {
                Category category = Category::Info;
                Localised english;
                Localised cantonese;
            };

            Category categoryFromString(const QString& value)
            {
                if (value == QLatin1String("success")) {
                    return Category::Success;
                }
                if (value == QLatin1String("warning")) {
                    return Category::Warning;
                }
                if (value == QLatin1String("error")) {
                    return Category::Error;
                }
                if (value == QLatin1String("destructive")) {
                    return Category::Destructive;
                }
                if (value == QLatin1String("security")) {
                    return Category::Security;
                }
                return Category::Info;
            }

            Localised readLocalised(const QJsonObject& object)
            {
                Localised localised;
                const QJsonObject levels = object.value(QStringLiteral("levels")).toObject();
                for (auto it = levels.constBegin(); it != levels.constEnd(); ++it) {
                    bool ok = false;
                    const int level = it.key().toInt(&ok);
                    if (ok && level >= MinLevel && level <= MaxLevel && !it.value().toString().isEmpty()) {
                        localised.levels.insert(level, it.value().toString());
                    }
                }
                const QJsonArray facts = object.value(QStringLiteral("facts")).toArray();
                for (const auto& fact : facts) {
                    if (!fact.toString().isEmpty()) {
                        localised.facts << fact.toString();
                    }
                }
                return localised;
            }

            const QMap<QString, Entry>& catalogue()
            {
                static const QMap<QString, Entry> loaded = [] {
                    initVoiceCatalogue();

                    QMap<QString, Entry> entries;
                    QFile file(QStringLiteral(":/voice/voice.json"));
                    if (!file.open(QIODevice::ReadOnly)) {
                        return entries;
                    }
                    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
                    const QJsonObject strings = root.value(QStringLiteral("strings")).toObject();
                    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it) {
                        const QJsonObject object = it.value().toObject();
                        Entry entry;
                        entry.category = categoryFromString(object.value(QStringLiteral("category")).toString());
                        entry.english = readLocalised(object.value(QStringLiteral("en")).toObject());
                        entry.cantonese = readLocalised(object.value(QStringLiteral("yue")).toObject());
                        entries.insert(it.key(), entry);
                    }
                    return entries;
                }();
                return loaded;
            }

            bool keepsFacts(const QString& text, const QStringList& facts)
            {
                for (const auto& fact : facts) {
                    if (!text.contains(fact)) {
                        return false;
                    }
                }
                return true;
            }

            /**
             * The bespoke line for @p level, or the nearest lower one that still
             * carries every fact. Level 1 is returned unconditionally when
             * nothing above it qualifies, so the result is never empty.
             */
            QString resolveTemplate(const Localised& localised, int level)
            {
                if (localised.levels.isEmpty()) {
                    return {};
                }
                for (int candidate = qBound(MinLevel, level, MaxLevel); candidate >= MinLevel; --candidate) {
                    const auto it = localised.levels.constFind(candidate);
                    if (it != localised.levels.constEnd() && keepsFacts(*it, localised.facts)) {
                        return *it;
                    }
                }
                const auto floor = localised.levels.constFind(MinLevel);
                return floor == localised.levels.constEnd() ? localised.levels.first() : *floor;
            }

            /** Placeholders with no argument are left intact, not silently dropped. */
            QString substitute(QString text, const QVariantMap& args)
            {
                for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
                    text.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value().toString());
                }
                return text;
            }

            Config::ConfigKey levelKey(Language language)
            {
                return language == Language::Cantonese ? Config::GUI_FunnyLevelCantonese
                                                       : Config::GUI_FunnyLevelEnglish;
            }
        } // namespace

        // ------------------------------------------------------------------ Notifier

        Notifier::Notifier() = default;

        Notifier* Notifier::instance()
        {
            static Notifier notifier;
            return &notifier;
        }

        void Notifier::announce()
        {
            emit changed();
        }

        // ---------------------------------------------------------------------- Line

        QString Line::joined() const
        {
            if (secondary.isEmpty()) {
                return primary;
            }
            return primary + QChar(BilingualSeparator) + secondary;
        }

        // ------------------------------------------------------------------ Lookup

        Line preview(Language language,
                     int englishLevel,
                     int cantoneseLevel,
                     const QString& key,
                     const QVariantMap& args,
                     Category category)
        {
            const auto it = catalogue().constFind(key);
            if (it == catalogue().constEnd()) {
                // A missing id is shown as its own name rather than as nothing.
                return Line{key, QString(), category};
            }

            // The caller may classify a message the catalogue does not know about;
            // leaving the default in place defers to the catalogue.
            const Category resolved = category == Category::Info ? it->category : category;

            const QString english = substitute(resolveTemplate(it->english, englishLevel), args);
            const QString cantonese = substitute(resolveTemplate(it->cantonese, cantoneseLevel), args);

            switch (language) {
            case Language::Cantonese:
                return Line{cantonese.isEmpty() ? english : cantonese, QString(), resolved};
            case Language::Bilingual:
                return Line{english, cantonese, resolved};
            case Language::English:
                break;
            }
            return Line{english, QString(), resolved};
        }

        Line line(const QString& key, const QVariantMap& args, Category category)
        {
            return preview(language(),
                           funnyLevel(Language::English),
                           funnyLevel(Language::Cantonese),
                           key,
                           args,
                           category);
        }

        QString say(const QString& key, Category category)
        {
            return line(key, {}, category).joined();
        }

        QString say(const QString& key, const QVariantMap& args, Category category)
        {
            return line(key, args, category).joined();
        }

        // ---------------------------------------------------------------- Settings

        Language language()
        {
            return languageFromString(config()->get(Config::GUI_VoiceLanguage).toString());
        }

        void setLanguage(Language language)
        {
            if (language == Voice::language()) {
                return;
            }
            config()->set(Config::GUI_VoiceLanguage, languageToString(language));
            notifier()->announce();
        }

        int funnyLevel(Language language)
        {
            return qBound(MinLevel, config()->get(levelKey(language)).toInt(), MaxLevel);
        }

        void setFunnyLevel(Language language, int level)
        {
            const int clamped = qBound(MinLevel, level, MaxLevel);
            if (clamped == funnyLevel(language)) {
                return;
            }
            config()->set(levelKey(language), clamped);
            notifier()->announce();
        }

        void resetToDefaults()
        {
            const bool changed = language() != Language::English || funnyLevel(Language::English) != DefaultLevel
                                 || funnyLevel(Language::Cantonese) != DefaultLevel;
            config()->set(Config::GUI_VoiceLanguage, languageToString(Language::English));
            config()->set(Config::GUI_FunnyLevelEnglish, DefaultLevel);
            config()->set(Config::GUI_FunnyLevelCantonese, DefaultLevel);
            if (changed) {
                notifier()->announce();
            }
        }

        // -------------------------------------------------------------- Vocabulary

        Language languageFromString(const QString& value)
        {
            if (value.compare(QLatin1String("Cantonese"), Qt::CaseInsensitive) == 0) {
                return Language::Cantonese;
            }
            if (value.compare(QLatin1String("Bilingual"), Qt::CaseInsensitive) == 0) {
                return Language::Bilingual;
            }
            return Language::English;
        }

        QString languageToString(Language language)
        {
            switch (language) {
            case Language::Cantonese:
                return QStringLiteral("Cantonese");
            case Language::Bilingual:
                return QStringLiteral("Bilingual");
            case Language::English:
                break;
            }
            return QStringLiteral("English");
        }

        QString languageDisplayName(Language language)
        {
            switch (language) {
            case Language::Cantonese:
                return QStringLiteral("廣東話");
            case Language::Bilingual:
                return QObject::tr("Both");
            case Language::English:
                break;
            }
            return QStringLiteral("English");
        }

        QString levelName(int level)
        {
            switch (qBound(MinLevel, level, MaxLevel)) {
            case 1:
                return QObject::tr("Professional");
            case 2:
                return QObject::tr("Matter of fact");
            case 3:
                return QObject::tr("Warm");
            case 4:
                return QObject::tr("Playful");
            default:
                break;
            }
            return QObject::tr("Maximum playfulness");
        }

        QString categoryToString(Category category)
        {
            switch (category) {
            case Category::Success:
                return QStringLiteral("success");
            case Category::Warning:
                return QStringLiteral("warning");
            case Category::Error:
                return QStringLiteral("error");
            case Category::Destructive:
                return QStringLiteral("destructive");
            case Category::Security:
                return QStringLiteral("security");
            case Category::Info:
                break;
            }
            return QStringLiteral("info");
        }

        QStringList catalogueKeys()
        {
            return catalogue().keys();
        }

        QStringList facts(const QString& key, Language language)
        {
            const auto it = catalogue().constFind(key);
            if (it == catalogue().constEnd()) {
                return {};
            }
            switch (language) {
            case Language::Cantonese:
                return it->cantonese.facts;
            case Language::Bilingual:
                return it->english.facts + it->cantonese.facts;
            case Language::English:
                break;
            }
            return it->english.facts;
        }

        // -------------------------------------------------------------- Disclosure

        QString disclosureText()
        {
            return QObject::tr(
                "The humour level styles every message in KeePassXC, including warnings, errors and the "
                "confirmations shown before something is deleted. It changes the wording only, never the facts: "
                "what happened, what it affects, which action cannot be undone and what the error actually was "
                "read the same at every level. Set either slider to 1 for a fully professional voice.");
        }

        bool disclosurePending()
        {
            return !config()->get(Config::GUI_VoiceDisclosureShown).toBool();
        }

        void presentDisclosure(QWidget* parent)
        {
            if (!parent) {
                return;
            }
            config()->set(Config::GUI_VoiceDisclosureShown, true);

            auto* dialog = new Dialog(parent);
            dialog->setSymbol(QStringLiteral("info"));
            dialog->setHeadline(QObject::tr("KeePassXC speaks with a voice"));
            dialog->setSupportingText(disclosureText());

            ButtonBase* professional = dialog->addAction(QObject::tr("Keep it professional"));
            QObject::connect(professional, &QAbstractButton::clicked, professional, [] {
                setFunnyLevel(Language::English, MinLevel);
                setFunnyLevel(Language::Cantonese, MinLevel);
            });
            dialog->addAction(QObject::tr("Got it"), true);

            QObject::connect(dialog, &Overlay::closed, dialog, &QObject::deleteLater);
            dialog->openOverlay();
        }

    } // namespace Voice

} // namespace Material
