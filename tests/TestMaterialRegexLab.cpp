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

#include "TestMaterialRegexLab.h"

#include "gui/material/MaterialRegexLab.h"

#include <QRegularExpression>
#include <QTest>

QTEST_GUILESS_MAIN(TestMaterialRegexLab)

using namespace Material::RegexLab;

void TestMaterialRegexLab::testTokenizeExplainsEveryConstruct()
{
    const auto tokens = tokenize(QStringLiteral("^(?<host>[^/:?#]+)\\d{2,}(?:a|b)*?\\1$"));
    QStringList types;
    for (const Token& token : tokens) {
        types << token.type;
        QVERIFY2(!token.english.isEmpty() && !token.cantonese.isEmpty(), qPrintable(token.text));
        QCOMPARE(token.end - token.start, token.text.size());
    }
    QCOMPARE(types,
             (QStringList{"anchor", "group", "charclass", "quant", "group", "class", "quant", "group", "literal", "alt", "literal", "group", "quant", "backref", "anchor"}));
    QCOMPARE(tokens.at(1).name, QStringLiteral("host"));
    QCOMPARE(tokens.at(1).group, 1);
    QVERIFY(tokens.at(6).english.contains(QStringLiteral("2 or more")));
    QVERIFY(tokens.at(12).english.contains(QStringLiteral("lazy")));
}

void TestMaterialRegexLab::testSpansCoverTheWholePattern()
{
    const QString pattern = QStringLiteral("\\b(?:25[0-5]|2[0-4]\\d)\\.(?>x)++\\p{Han}\\k<n>");
    const auto tokens = tokenize(pattern);
    int position = 0;
    for (const Token& token : tokens) {
        QCOMPARE(token.start, position);
        position = token.end;
    }
    QCOMPARE(position, pattern.size());
    // PCRE2-only constructs are flagged so the dialect view can say so.
    int pcreOnly = 0;
    for (const Token& token : tokens) {
        pcreOnly += token.pcreOnly ? 1 : 0;
    }
    QCOMPARE(pcreOnly, 2);
}

void TestMaterialRegexLab::testTranslateWritesEveryDialect()
{
    const auto exports = translate(QStringLiteral("\\d+\"x\""), QStringLiteral("gi"));
    QCOMPARE(exports.size(), 4);
    QCOMPARE(exports.at(0).id, QStringLiteral("qt"));
    QVERIFY(exports.at(0).code.contains(QStringLiteral("QStringLiteral(\"\\\\d+\\\"x\\\"\")")));
    QVERIFY(exports.at(0).code.contains(QStringLiteral("CaseInsensitiveOption")));
    QCOMPARE(exports.at(1).code, QStringLiteral("/\\d+\"x\"/gi"));
    QVERIFY(exports.at(2).code.startsWith(QStringLiteral("re.compile(r\"")));
    QVERIFY(exports.at(2).code.contains(QStringLiteral("re.IGNORECASE")));
    QVERIFY(exports.at(3).code.startsWith(QStringLiteral("grep -Pi ")));
}

void TestMaterialRegexLab::testQtReplacementResolvesNames()
{
    const QString pattern = QStringLiteral("(?<local>[\\w.]+)@(?<domain>[\\w.]+)");
    QCOMPARE(qtReplacement(pattern, QStringLiteral("$<domain>/$<local> $& $$ $2")),
             QStringLiteral("\\2/\\1 \\0 $ \\2"));
    // The translated form really works in QString::replace.
    QString sample = QStringLiteral("ops@acme.example");
    sample.replace(QRegularExpression(pattern), qtReplacement(pattern, QStringLiteral("$<domain>:$1")));
    QCOMPARE(sample, QStringLiteral("acme.example:ops"));
    // An unknown name is left as typed rather than silently dropped.
    QCOMPARE(qtReplacement(pattern, QStringLiteral("$<nope>")), QStringLiteral("$<nope>"));
}

void TestMaterialRegexLab::testPresetsCompileAndMatchTheirSamples()
{
    const auto library = presets();
    QCOMPARE(library.size(), 10);
    for (const Preset& preset : library) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (preset.flags.contains(QLatin1Char('i'))) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        if (preset.flags.contains(QLatin1Char('m'))) {
            options |= QRegularExpression::MultilineOption;
        }
        if (preset.flags.contains(QLatin1Char('u'))) {
            options |= QRegularExpression::UseUnicodePropertiesOption;
        }
        const QRegularExpression regex(preset.pattern, options);
        QVERIFY2(regex.isValid(), qPrintable(preset.id + QStringLiteral(": ") + regex.errorString()));
        QVERIFY2(regex.match(preset.sample).hasMatch(), qPrintable(preset.id));
        QVERIFY(!preset.name.isEmpty() && !preset.cantonese.isEmpty());
    }
}

void TestMaterialRegexLab::testDialectsAndCheatSheetAreComplete()
{
    const auto engines = dialects();
    QCOMPARE(engines.size(), 2);
    QCOMPARE(engines.at(0).id, QStringLiteral("js"));
    QCOMPARE(engines.at(1).id, QStringLiteral("qt"));
    for (const Dialect& dialect : engines) {
        QCOMPARE(dialect.flags.size(), 6);
        QCOMPARE(dialect.notes.size(), 3);
    }
    const auto sheet = cheatSheet();
    QVERIFY(sheet.size() >= 30);
    int pcreOnly = 0;
    for (const CheatEntry& row : sheet) {
        QVERIFY2(!row.token.isEmpty() && !row.english.isEmpty() && !row.cantonese.isEmpty(), qPrintable(row.token));
        pcreOnly += row.pcreOnly ? 1 : 0;
    }
    QCOMPARE(pcreOnly, 3);
}
