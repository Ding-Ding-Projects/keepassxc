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

#include "PersonalVocabulary.h"

#include "core/Config.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QList>
#include <QPointer>
#include <QRegularExpression>

#include <algorithm>

namespace
{
    QList<QPointer<QTranslator>>& baseTranslators()
    {
        static QList<QPointer<QTranslator>> list;
        return list;
    }

    QPointer<PersonalVocabularyTranslator>& installedTranslator()
    {
        static QPointer<PersonalVocabularyTranslator> pointer;
        return pointer;
    }

    bool unsafeKey(const QString& key)
    {
        return key == QLatin1String("__proto__") || key == QLatin1String("constructor")
               || key == QLatin1String("prototype");
    }
} // namespace

namespace PersonalVocabulary
{
    Validation validate(const QByteArray& bytes)
    {
        Validation result;
        if (bytes.size() > MaxFileBytes) {
            result.reason = QStringLiteral("file larger than 64 KiB");
            return result;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            result.reason = QStringLiteral("not a JSON object");
            return result;
        }
        const QJsonObject root = document.object();
        if (root.size() != 2 || root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
            result.reason = QStringLiteral("schemaVersion must be 1 and the object must carry exactly two members");
            return result;
        }
        const QString entriesKey = root.contains(QStringLiteral("entries")) ? QStringLiteral("entries")
                                                                              : QStringLiteral("replacements");
        if (!root.contains(entriesKey) || !root.value(entriesKey).isObject()) {
            result.reason = QStringLiteral("entries must be an object");
            return result;
        }
        const QJsonObject entries = root.value(entriesKey).toObject();
        if (entries.size() > MaxEntries) {
            result.reason = QStringLiteral("more than 500 entries");
            return result;
        }
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (!it.value().isString()) {
                result.reason = QStringLiteral("entry values must be strings");
                return result;
            }
            if (it.key().isEmpty() || it.key().size() > MaxKeyLength || it.value().toString().size() > MaxValueLength
                || unsafeKey(it.key())) {
                result.reason = QStringLiteral("entry key or value out of bounds");
                return result;
            }
        }
        result.valid = true;
        result.canonical = QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("entries"), entries}};
        return result;
    }

    QHash<QString, QString> entriesFromCache(const QString& cache)
    {
        QHash<QString, QString> entries;
        if (cache.isEmpty()) {
            return entries;
        }
        const Validation validation = validate(cache.toUtf8());
        if (!validation.valid) {
            return entries;
        }
        const QJsonObject object = validation.canonical.value(QStringLiteral("entries")).toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            entries.insert(it.key(), it.value().toString());
        }
        return entries;
    }

    QString apply(const QString& text, const QHash<QString, QString>& entries)
    {
        if (entries.isEmpty() || text.isEmpty()) {
            return text;
        }
        QStringList keys = entries.keys();
        std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {
            return a.size() != b.size() ? a.size() > b.size() : a < b;
        });
        QString result = text;
        for (const QString& key : keys) {
            if (!result.contains(key)) {
                continue;
            }
            const QRegularExpression pattern(QStringLiteral("(?<![\\p{L}\\p{N}_])") + QRegularExpression::escape(key)
                                             + QStringLiteral("(?![\\p{L}\\p{N}_])"));
            result.replace(pattern, entries.value(key));
        }
        return result;
    }

    int activeEntryCount()
    {
        if (!config()) {
            return 0;
        }
        return entriesFromCache(config()->get(Config::GUI_PersonalVocabularyCache).toString()).size();
    }

    void refresh()
    {
        if (installedTranslator()) {
            installedTranslator()->reload();
        }
    }

    void registerBaseTranslator(QTranslator* translator)
    {
        if (translator) {
            baseTranslators().append(translator);
        }
    }

    void install()
    {
        if (!qApp || installedTranslator()) {
            return;
        }
        auto* translator = new PersonalVocabularyTranslator(qApp);
        installedTranslator() = translator;
        QCoreApplication::installTranslator(translator);
    }
} // namespace PersonalVocabulary

PersonalVocabularyTranslator::PersonalVocabularyTranslator(QObject* parent)
    : QTranslator(parent)
{
    reload();
}

void PersonalVocabularyTranslator::reload()
{
    m_entries = config() ? PersonalVocabulary::entriesFromCache(config()->get(Config::GUI_PersonalVocabularyCache).toString())
                         : QHash<QString, QString>();
}

bool PersonalVocabularyTranslator::isEmpty() const
{
    return false;
}

QString PersonalVocabularyTranslator::translate(const char* context,
                                                const char* sourceText,
                                                const char* disambiguation,
                                                int n) const
{
    if (m_entries.isEmpty() || !sourceText) {
        return QString();
    }
    QString base;
    for (const QPointer<QTranslator>& translator : baseTranslators()) {
        if (translator) {
            base = translator->translate(context, sourceText, disambiguation, n);
            if (!base.isEmpty()) {
                break;
            }
        }
    }
    if (base.isEmpty()) {
        base = QString::fromUtf8(sourceText);
    }
    return PersonalVocabulary::apply(base, m_entries);
}
