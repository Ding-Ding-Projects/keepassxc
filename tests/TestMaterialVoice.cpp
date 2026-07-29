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

#include "TestMaterialVoice.h"

#include "core/Config.h"
#include "gui/material/MaterialVoice.h"

#include <QTest>

QTEST_GUILESS_MAIN(TestMaterialVoice)

using Material::Voice::Category;
using Material::Voice::Language;
using Material::Voice::Line;
namespace Voice = Material::Voice;

namespace
{
    const QList<Language> AllLanguages{Language::English, Language::Cantonese, Language::Bilingual};

    /** Arguments covering every placeholder the starter catalogue uses. */
    QVariantMap sampleArgs()
    {
        return QVariantMap{{QStringLiteral("seconds"), 10},
                           {QStringLiteral("count"), 2},
                           {QStringLiteral("name"), QStringLiteral("Personal.kdbx")},
                           {QStringLiteral("title"), QStringLiteral("GitHub")},
                           {QStringLiteral("window"), QStringLiteral("Firefox")},
                           {QStringLiteral("path"), QStringLiteral("/tmp/report.csv")},
                           {QStringLiteral("error"), QStringLiteral("no space left on device")}};
    }
} // namespace

void TestMaterialVoice::initTestCase()
{
    QVERIFY(m_configDir.isValid());
    Config::createConfigFromFile(m_configDir.filePath(QStringLiteral("voice.ini")));
}

void TestMaterialVoice::init()
{
    Voice::resetToDefaults();
}

void TestMaterialVoice::testCatalogueCoversTheRequiredKeys()
{
    const QStringList required{QStringLiteral("clipboard.copied"),
                               QStringLiteral("database.saved"),
                               QStringLiteral("database.locked"),
                               QStringLiteral("entry.deleted"),
                               QStringLiteral("entry.restored"),
                               QStringLiteral("autotype.sent"),
                               QStringLiteral("report.exported"),
                               QStringLiteral("passkey.copied.warning"),
                               QStringLiteral("passkey.imported"),
                               QStringLiteral("database.save.failed"),
                               QStringLiteral("database.open.failed"),
                               QStringLiteral("entry.delete.confirm")};

    const QStringList keys = Voice::catalogueKeys();
    QVERIFY(!keys.isEmpty());
    for (const auto& key : required) {
        QVERIFY2(keys.contains(key), qPrintable(QStringLiteral("missing catalogue id %1").arg(key)));
    }
}

void TestMaterialVoice::testEveryKeyResolvesInEveryLanguageAtEveryLevel()
{
    const QVariantMap args = sampleArgs();

    for (const auto& key : Voice::catalogueKeys()) {
        for (auto language : AllLanguages) {
            Voice::setLanguage(language);
            for (int level = Voice::MinLevel; level <= Voice::MaxLevel; ++level) {
                Voice::setFunnyLevel(Language::English, level);
                Voice::setFunnyLevel(Language::Cantonese, level);

                const QString message = QStringLiteral("%1 @ %2 level %3")
                                            .arg(key, Voice::languageToString(language))
                                            .arg(level);

                const Line line = Voice::line(key, args, Category::Info);
                QVERIFY2(!line.primary.trimmed().isEmpty(), qPrintable(message));
                QVERIFY2(!Voice::say(key, args, Category::Info).trimmed().isEmpty(), qPrintable(message));
                // No placeholder is left unresolved once every argument is supplied.
                QVERIFY2(!line.joined().contains(QLatin1Char('{')), qPrintable(message));
            }
        }
    }
}

void TestMaterialVoice::testFallbackChain()
{
    const QVariantMap args = sampleArgs();
    const QString key = QStringLiteral("entry.delete.confirm");

    // The catalogue carries bespoke lines at 1, 3 and 5 only.
    for (auto language : {Language::English, Language::Cantonese}) {
        const QString level1 = Voice::preview(language, 1, 1, key, args).primary;
        const QString level2 = Voice::preview(language, 2, 2, key, args).primary;
        const QString level3 = Voice::preview(language, 3, 3, key, args).primary;
        const QString level4 = Voice::preview(language, 4, 4, key, args).primary;
        const QString level5 = Voice::preview(language, 5, 5, key, args).primary;

        QCOMPARE(level2, level1);
        QCOMPARE(level4, level3);
        QVERIFY(level3 != level1);
        QVERIFY(level5 != level3);
    }

    // Out of range levels clamp onto the ends rather than resolving to nothing.
    QCOMPARE(Voice::preview(Language::English, 0, 0, key, args).primary,
             Voice::preview(Language::English, 1, 1, key, args).primary);
    QCOMPARE(Voice::preview(Language::English, 99, 99, key, args).primary,
             Voice::preview(Language::English, 5, 5, key, args).primary);

    // An id the catalogue does not know still answers with something non-empty.
    QCOMPARE(Voice::say(QStringLiteral("no.such.id")), QStringLiteral("no.such.id"));
}

void TestMaterialVoice::testBilingualCarriesBothParts()
{
    const QVariantMap args = sampleArgs();

    for (const auto& key : Voice::catalogueKeys()) {
        for (int level = Voice::MinLevel; level <= Voice::MaxLevel; ++level) {
            const QString english = Voice::preview(Language::English, level, level, key, args).primary;
            const QString cantonese = Voice::preview(Language::Cantonese, level, level, key, args).primary;
            const Line both = Voice::preview(Language::Bilingual, level, level, key, args);

            QVERIFY(both.hasSecondary());
            QCOMPARE(both.primary, english);
            QCOMPARE(both.secondary, cantonese);
            QVERIFY(english != cantonese);

            const QString joined = both.joined();
            QVERIFY(joined.contains(english));
            QVERIFY(joined.contains(cantonese));
        }
    }
}

void TestMaterialVoice::testFactsSurviveEveryLevel()
{
    // Facts are declared on the raw templates, so they are checked before the
    // arguments are substituted in.
    for (const auto& key : Voice::catalogueKeys()) {
        for (auto language : {Language::English, Language::Cantonese}) {
            const QStringList facts = Voice::facts(key, language);
            QVERIFY2(!facts.isEmpty(), qPrintable(QStringLiteral("%1 declares no facts").arg(key)));

            for (int level = Voice::MinLevel; level <= Voice::MaxLevel; ++level) {
                const QString text = Voice::preview(language, level, level, key).primary;
                for (const auto& fact : facts) {
                    QVERIFY2(text.contains(fact),
                             qPrintable(QStringLiteral("%1 @ %2 level %3 dropped the fact \"%4\"")
                                            .arg(key, Voice::languageToString(language))
                                            .arg(level)
                                            .arg(fact)));
                }
            }
        }
    }
}

void TestMaterialVoice::testDestructiveMessageKeepsTheIrreversibleAction()
{
    const QVariantMap args = sampleArgs();
    const QString key = QStringLiteral("entry.delete.confirm");

    const QString plain = Voice::preview(Language::English, 1, 1, key, args).primary;
    const QString funny = Voice::preview(Language::English, 5, 5, key, args).primary;

    // The joke is allowed to change the sentence around it, never the warning.
    QVERIFY(plain != funny);
    for (const auto& fact : {QStringLiteral("permanently delete the entry"),
                             QStringLiteral("This cannot be undone"),
                             QStringLiteral("GitHub")}) {
        QVERIFY2(plain.contains(fact), qPrintable(fact));
        QVERIFY2(funny.contains(fact), qPrintable(fact));
    }

    const QString plainYue = Voice::preview(Language::Cantonese, 1, 1, key, args).primary;
    const QString funnyYue = Voice::preview(Language::Cantonese, 5, 5, key, args).primary;
    QVERIFY(plainYue != funnyYue);
    for (const auto& fact :
         {QString::fromUtf8("永久刪除項目"), QString::fromUtf8("此操作無法復原"), QStringLiteral("GitHub")}) {
        QVERIFY(plainYue.contains(fact));
        QVERIFY(funnyYue.contains(fact));
    }

    // The destructive category never softens or suppresses the level.
    QCOMPARE(Voice::preview(Language::English, 5, 5, key, args, Category::Destructive).primary, funny);
}

void TestMaterialVoice::testLevelIsPersistedPerLanguage()
{
    Voice::setFunnyLevel(Language::English, 1);
    Voice::setFunnyLevel(Language::Cantonese, 5);

    QCOMPARE(Voice::funnyLevel(Language::English), 1);
    QCOMPARE(Voice::funnyLevel(Language::Cantonese), 5);
    QCOMPARE(config()->get(Config::GUI_FunnyLevelEnglish).toInt(), 1);
    QCOMPARE(config()->get(Config::GUI_FunnyLevelCantonese).toInt(), 5);

    // Bilingual has no slider of its own and answers with the English one.
    QCOMPARE(Voice::funnyLevel(Language::Bilingual), 1);

    // Out of range values are clamped rather than stored.
    Voice::setFunnyLevel(Language::English, 42);
    QCOMPARE(Voice::funnyLevel(Language::English), Voice::MaxLevel);

    const QString key = QStringLiteral("database.locked");
    Voice::setFunnyLevel(Language::English, 1);
    Voice::setFunnyLevel(Language::Cantonese, 5);
    const Line line = Voice::line(key, sampleArgs());
    QCOMPARE(line.primary, Voice::preview(Language::English, 1, 5, key, sampleArgs()).primary);

    Voice::setLanguage(Language::Bilingual);
    QCOMPARE(Voice::language(), Language::Bilingual);
    QCOMPARE(config()->get(Config::GUI_VoiceLanguage).toString(), QStringLiteral("Bilingual"));
}

void TestMaterialVoice::testResetToDefaults()
{
    Voice::setLanguage(Language::Cantonese);
    Voice::setFunnyLevel(Language::English, 5);
    Voice::setFunnyLevel(Language::Cantonese, 1);

    Voice::resetToDefaults();

    QCOMPARE(Voice::language(), Language::English);
    QCOMPARE(Voice::funnyLevel(Language::English), Voice::DefaultLevel);
    QCOMPARE(Voice::funnyLevel(Language::Cantonese), Voice::DefaultLevel);
    QVERIFY(!Voice::disclosureText().trimmed().isEmpty());
}
