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

#include "TestDimSum.h"

#include "core/Config.h"
#include "gui/material/MaterialDimSum.h"
#include "util/TemporaryFile.h"

#include <QFile>
#include <QLocale>
#include <QSet>
#include <QStringList>
#include <QSvgRenderer>
#include <QTest>
#include <QWidget>

QTEST_MAIN(TestDimSum)

using Material::DimSum;
using Material::DimSumCard;

void TestDimSum::initTestCase()
{
    QLocale::setDefault(QLocale::c());
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), {});

    // A configuration that has never seen a database reads as a first run, and
    // the surprise stands down on those. Give it a history so the rules under
    // test are the ones actually being exercised.
    config()->set(Config::LastDatabases, QStringList{QStringLiteral("/tmp/dimsum.kdbx")});
    config()->set(Config::GUI_MinimizeOnStartup, false);
    config()->sync();
}

void TestDimSum::init()
{
    // Every rule under test latches for the launch: the draw, the card that was
    // shown, the pending timer, the suppression. All five test functions run in
    // one process, so without this each of them would be reading whatever the
    // previous one left behind instead of the rule it means to exercise -
    // testFiresOnlyOncePerLaunch in particular used to pass on a draw that
    // testDisabledSuppressesAbsolutely had already latched to false.
    DimSum::resetLaunchState();
    QVERIFY(!DimSum::hasShown());

    config()->set(Config::GUI_DimSumSurprise, true);

    m_window.reset(new QWidget);
    m_window->resize(1000, 700);
    m_window->show();
    m_window->activateWindow();
    QTest::qWait(10);
}

void TestDimSum::cleanup()
{
    config()->set(Config::GUI_DimSumSurprise, true);
    m_window.reset();
}

void TestDimSum::testCatalogueParses()
{
    const auto dishes = DimSum::catalogue();
    QVERIFY2(dishes.size() >= 10, "the bundled catalogue should carry at least ten dishes");

    // The catalogue is parsed once and handed out by value; it must not drift.
    QCOMPARE(DimSum::catalogue().size(), dishes.size());
}

void TestDimSum::testEveryDishHasBothNamesAndArt()
{
    QSet<QString> seenEnglish;
    QSet<QString> seenCantonese;

    const auto dishes = DimSum::catalogue();
    for (const auto& dish : dishes) {
        QVERIFY2(!dish.english.isEmpty(), qPrintable(QStringLiteral("missing English name for %1").arg(dish.asset)));
        QVERIFY2(!dish.cantonese.isEmpty(), qPrintable(QStringLiteral("missing Cantonese name for %1").arg(dish.asset)));
        QVERIFY(dish.isValid());

        QVERIFY2(dish.asset.startsWith(QLatin1String(":/dimsum/")),
                 qPrintable(QStringLiteral("%1 is not a bundled asset").arg(dish.asset)));
        QVERIFY2(QFile::exists(dish.asset),
                 qPrintable(QStringLiteral("%1 is not in the qrc").arg(dish.asset)));

        // Present in the qrc is not the same as drawable.
        QSvgRenderer renderer(dish.asset);
        QVERIFY2(renderer.isValid(), qPrintable(QStringLiteral("%1 does not parse as SVG").arg(dish.asset)));
        QVERIFY(!renderer.defaultSize().isEmpty());

        // A dish listed twice would skew the draw.
        QVERIFY(!seenEnglish.contains(dish.english));
        QVERIFY(!seenCantonese.contains(dish.cantonese));
        seenEnglish.insert(dish.english);
        seenCantonese.insert(dish.cantonese);
    }
}

void TestDimSum::testDisplayNameCarriesBothLanguages()
{
    const auto dishes = DimSum::catalogue();
    for (const auto& dish : dishes) {
        const QString name = dish.displayName();
        QVERIFY2(name.contains(dish.english), qPrintable(name));
        QVERIFY2(name.contains(dish.cantonese), qPrintable(name));
    }
}

void TestDimSum::testRetiredOptOutIsIgnored()
{
    // The surprise has no opt-out. Whatever this desktop currently allows (quiet
    // hours and focus assist are themselves part of the contract), the retired
    // GUI_DimSumSurprise key must make no difference to the answer.
    DimSum::resetLaunchState();
    config()->set(Config::GUI_DimSumSurprise, true);
    const bool withKeyOn = DimSum::showNow(m_window.data());
    const bool shownOn = DimSum::hasShown();

    DimSum::resetLaunchState();
    config()->set(Config::GUI_DimSumSurprise, false);
    const bool withKeyOff = DimSum::showNow(m_window.data());
    QCOMPARE(withKeyOff, withKeyOn);
    QCOMPARE(DimSum::hasShown(), shownOn);

    // The launch decision still latches: shouldShow() answers the same thing
    // however often it is asked, which keeps the odds honest and keeps 20,000
    // calls a bool read rather than 42 minutes of shell queries.
    const bool first = DimSum::shouldShow();
    for (int i = 0; i < 20000; ++i) {
        QCOMPARE(DimSum::shouldShow(), first);
    }
}

void TestDimSum::testFiresOnlyOncePerLaunch()
{
    const bool first = DimSum::showNow(m_window.data());
    if (!first) {
        // Quiet hours, focus assist or an inactive window: the card is being
        // suppressed for a reason that is itself part of the contract.
        QVERIFY(!DimSum::hasShown());
        QCOMPARE(m_window->findChildren<DimSumCard*>().size(), 0);
        QSKIP("The desktop is not in a state that accepts an unprompted card.");
    }

    QVERIFY(DimSum::hasShown());
    QCOMPARE(m_window->findChildren<DimSumCard*>().size(), 1);

    // init() cleared the launch state, so the card above is this launch's one
    // surprise and every refusal below is the once-per-launch rule refusing,
    // not a stale latch left over from an earlier test function.
    for (int i = 0; i < 200; ++i) {
        QVERIFY(!DimSum::shouldShow());
        QVERIFY(!DimSum::showNow(m_window.data()));
        DimSum::showIfDue(m_window.data());
    }
    QTest::qWait(20);

    QCOMPARE(m_window->findChildren<DimSumCard*>().size(), 1);
}
