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

#include "TestPersonalVocabulary.h"

#include "core/Config.h"
#include "core/PersonalVocabulary.h"
#include "util/TemporaryFile.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

QTEST_MAIN(TestPersonalVocabulary)

namespace
{
    QByteArray file(const QJsonObject& entries, int schemaVersion = 1, const QString& key = QStringLiteral("entries"))
    {
        return QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), schemaVersion}, {key, entries}}).toJson();
    }
} // namespace

void TestPersonalVocabulary::initTestCase()
{
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});
}

void TestPersonalVocabulary::validatesBoundedSchema()
{
    const QJsonObject good{{QStringLiteral("Database"), QStringLiteral("Vault")}};
    QVERIFY(PersonalVocabulary::validate(file(good)).valid);
    // The earlier key name still loads and is normalised to entries.
    const auto legacy = PersonalVocabulary::validate(file(good, 1, QStringLiteral("replacements")));
    QVERIFY(legacy.valid);
    QVERIFY(legacy.canonical.contains(QStringLiteral("entries")));

    QVERIFY(!PersonalVocabulary::validate(QByteArray("not json")).valid);
    QVERIFY(!PersonalVocabulary::validate(QByteArray("[]")).valid);
    QVERIFY(!PersonalVocabulary::validate(file(good, 2)).valid);
    QVERIFY(!PersonalVocabulary::validate(QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}}).toJson()).valid);
    QVERIFY(!PersonalVocabulary::validate(QByteArray(PersonalVocabulary::MaxFileBytes + 1, 'x')).valid);
    QVERIFY(!PersonalVocabulary::validate(file(QJsonObject{{QStringLiteral("Database"), 3}})).valid);
    QVERIFY(!PersonalVocabulary::validate(file(QJsonObject{{QStringLiteral("__proto__"), QStringLiteral("x")}})).valid);
    QVERIFY(!PersonalVocabulary::validate(file(QJsonObject{{QString(129, 'k'), QStringLiteral("x")}})).valid);
    QVERIFY(!PersonalVocabulary::validate(file(QJsonObject{{QStringLiteral("k"), QString(513, 'v')}})).valid);
    QJsonObject tooMany;
    for (int index = 0; index <= PersonalVocabulary::MaxEntries; ++index) {
        tooMany.insert(QStringLiteral("key%1").arg(index), QStringLiteral("v"));
    }
    QVERIFY(!PersonalVocabulary::validate(file(tooMany)).valid);
    // Extra members are refused; nothing outside the declared schema applies.
    QJsonObject extra{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("entries"), good}, {QStringLiteral("x"), 1}};
    QVERIFY(!PersonalVocabulary::validate(QJsonDocument(extra).toJson()).valid);
}

void TestPersonalVocabulary::appliesWholeWordsLongestFirst()
{
    const QHash<QString, QString> entries{{QStringLiteral("Database"), QStringLiteral("Vault")},
                                          {QStringLiteral("Database file"), QStringLiteral("Vault bundle")}};
    QCOMPARE(PersonalVocabulary::apply(QStringLiteral("Open Database file"), entries), QStringLiteral("Open Vault bundle"));
    QCOMPARE(PersonalVocabulary::apply(QStringLiteral("Lock Database now"), entries), QStringLiteral("Lock Vault now"));
    // Whole words only: an identifier containing the key is untouched.
    QCOMPARE(PersonalVocabulary::apply(QStringLiteral("DatabaseWidget"), entries), QStringLiteral("DatabaseWidget"));
    QCOMPARE(PersonalVocabulary::apply(QStringLiteral("database"), entries), QStringLiteral("database"));
    QCOMPARE(PersonalVocabulary::apply(QString(), entries), QString());
}

void TestPersonalVocabulary::translatorAppliesCacheAndClearRestoresWording()
{
    PersonalVocabulary::install();
    config()->remove(Config::GUI_PersonalVocabularyCache);
    PersonalVocabulary::refresh();
    QCOMPARE(PersonalVocabulary::activeEntryCount(), 0);
    QCOMPARE(QCoreApplication::translate("Test", "Unlock Database"), QStringLiteral("Unlock Database"));

    const auto validation = PersonalVocabulary::validate(file(QJsonObject{{QStringLiteral("Database"), QStringLiteral("Vault")}}));
    QVERIFY(validation.valid);
    config()->set(Config::GUI_PersonalVocabularyCache,
                  QString::fromUtf8(QJsonDocument(validation.canonical).toJson(QJsonDocument::Compact)));
    PersonalVocabulary::refresh();
    QCOMPARE(PersonalVocabulary::activeEntryCount(), 1);
    QCOMPARE(QCoreApplication::translate("Test", "Unlock Database"), QStringLiteral("Unlock Vault"));
    // Replace: a new file supersedes the old cache entirely.
    const auto replaced = PersonalVocabulary::validate(file(QJsonObject{{QStringLiteral("Unlock"), QStringLiteral("Open")}}));
    config()->set(Config::GUI_PersonalVocabularyCache,
                  QString::fromUtf8(QJsonDocument(replaced.canonical).toJson(QJsonDocument::Compact)));
    PersonalVocabulary::refresh();
    QCOMPARE(QCoreApplication::translate("Test", "Unlock Database"), QStringLiteral("Open Database"));
    // A corrupt cache fails closed to the original wording.
    config()->set(Config::GUI_PersonalVocabularyCache, QStringLiteral("{corrupt"));
    PersonalVocabulary::refresh();
    QCOMPARE(PersonalVocabulary::activeEntryCount(), 0);
    QCOMPARE(QCoreApplication::translate("Test", "Unlock Database"), QStringLiteral("Unlock Database"));
    // Clear restores the original wording.
    config()->remove(Config::GUI_PersonalVocabularyCache);
    PersonalVocabulary::refresh();
    QCOMPARE(QCoreApplication::translate("Test", "Unlock Database"), QStringLiteral("Unlock Database"));
}
