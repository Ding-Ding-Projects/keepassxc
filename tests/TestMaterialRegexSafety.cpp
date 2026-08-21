#include "TestMaterialRegexSafety.h"

#include "gui/material/MaterialRegexSafety.h"

#include <QTest>

using namespace Material;

void TestMaterialRegexSafety::validAndInvalidPatterns()
{
    auto run = runBounded(QStringLiteral("root|admin"), optionsForFlags(QStringLiteral("i")), QStringLiteral("ROOT user"));
    QVERIFY(run.compiled);
    QCOMPARE(run.matches.size(), 1);
    run = runBounded(QStringLiteral("("), {}, QStringLiteral("text"));
    QVERIFY(!run.compiled);
    QVERIFY(!run.error.isEmpty());
}

void TestMaterialRegexSafety::boundsAndRiskShapes()
{
    auto run = runBounded(QString(RegexLimits::PatternChars + 1, QLatin1Char('a')), {}, QStringLiteral("a"));
    QVERIFY(!run.compiled);
    run = runBounded(QStringLiteral("(a+)+$"), {}, QString(100, QLatin1Char('a')));
    QVERIFY(run.blocked);
    QVERIFY(!run.error.isEmpty());
    run = runBounded(QStringLiteral("a"), {}, QString(RegexLimits::SampleChars + 1, QLatin1Char('a')));
    QVERIFY(run.sampleTruncated);
}

void TestMaterialRegexSafety::zeroWidthAndMatchLimit()
{
    auto run = runBounded(QStringLiteral("(?=a)"), {}, QStringLiteral("aaa"));
    QVERIFY(run.compiled);
    QCOMPARE(run.matches.size(), 3);
    run = runBounded(QStringLiteral("a"), {}, QString(RegexLimits::MaxMatches + 20, QLatin1Char('a')));
    QCOMPARE(run.matches.size(), RegexLimits::MaxMatches);
    QVERIFY(run.truncated);
}

QTEST_GUILESS_MAIN(TestMaterialRegexSafety)
