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

#ifndef KEEPASSX_PERSONALVOCABULARY_H
#define KEEPASSX_PERSONALVOCABULARY_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QTranslator>

class QTranslator;

/**
 * Local-only personal vocabulary: a bounded, versioned JSON file the user
 * supplies through Settings. Its validated copy lives in the local config
 * only; nothing here reads the network, logs a mapping or exports the cache.
 *
 * Contract (schema version 1): an object with exactly two members,
 *   "schemaVersion": 1 and "entries": { "<original>": "<replacement>", ... }
 * with at most 500 entries, keys 1..128 characters, values 0..512 characters,
 * string values only, no unsafe object keys, and a file no larger than 64 KiB.
 * "replacements" is accepted as an alias of "entries" for files written by
 * earlier builds.
 */
namespace PersonalVocabulary
{
    constexpr int MaxFileBytes = 65536;
    constexpr int MaxEntries = 500;
    constexpr int MaxKeyLength = 128;
    constexpr int MaxValueLength = 512;

    struct Validation
    {
        bool valid = false;
        QString reason;
        QJsonObject canonical; // schemaVersion + entries, ready to cache
    };

    /// Validate raw file bytes against the contract without applying them.
    Validation validate(const QByteArray& bytes);

    /// Parse the cached canonical JSON into replacement pairs (empty when none or invalid).
    QHash<QString, QString> entriesFromCache(const QString& cache);

    /// Apply whole-word, case-sensitive replacements (longest key first).
    QString apply(const QString& text, const QHash<QString, QString>& entries);

    /// Re-read the local cache after an upload or a clear; the caller re-translates its windows.
    void refresh();

    /// Number of active entries (0 when no valid cache is present).
    int activeEntryCount();

    /// Install the vocabulary translator after the language translators.
    void install();

    /// Record a language translator the vocabulary translator consults first.
    void registerBaseTranslator(QTranslator* translator);
} // namespace PersonalVocabulary

/**
 * Translator installed last so it answers first: it asks the language
 * translators for the real string, then applies the personal vocabulary at
 * that user-facing text boundary. With no vocabulary it answers nothing and
 * the ordinary chain runs unchanged.
 */
class PersonalVocabularyTranslator : public QTranslator
{
    Q_OBJECT
public:
    explicit PersonalVocabularyTranslator(QObject* parent = nullptr);
    QString translate(const char* context, const char* sourceText, const char* disambiguation, int n) const override;
    bool isEmpty() const override;
    void reload();

private:
    QHash<QString, QString> m_entries;
};

#endif // KEEPASSX_PERSONALVOCABULARY_H
