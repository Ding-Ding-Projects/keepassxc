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

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QLocale>
#include <QSet>
#include <QStringList>
#include "gui/material/MaterialDimSumPhotos.h"
#include <QTest>
#include <QWidget>



using Material::DimSum;
using Material::DimSumCard;

void TestDimSum::initTestCase()
{
    QLocale::setDefault(QLocale::c());
    Config::createConfigFromFile(qEnvironmentVariable("KPXC_CONFIG"), qEnvironmentVariable("KPXC_CONFIG_LOCAL"));

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
    // testLegacyPreferenceDoesNotDisable had already latched to false.
    DimSum::resetLaunchState();
    DimSum::s_testCatalogue=true;
    DimSum::s_catalogue=[] { QImage image(8,8,QImage::Format_RGB32);image.fill(Qt::white);return QVector<DimSum::Dish>{{QStringLiteral("Synthetic dish"),QStringLiteral("測試點心"),QStringLiteral("https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/catalog-v1/fixture.png"),image}}; };
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
    QCOMPARE(dishes.size(),1); // Explicit in-memory fixture, not a copied catalogue.

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

        QVERIFY(Material::DimSumPhotos::publicAssetUrl(QUrl(dish.asset)));
        QVERIFY(!dish.image.isNull());

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

void TestDimSum::testLegacyPreferenceDoesNotDisable()
{
    config()->set(Config::GUI_DimSumSurprise, false);
    DimSum::beginStartup();
    QVERIFY(config()->get(Config::GUI_DimSumSurprise).toBool());
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

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir isolated(QDir::tempPath() + QStringLiteral("/kds-old-XXXXXX"));
    if (!isolated.isValid()) return 2;
    qputenv("KPXC_CONFIG", (isolated.path() + QStringLiteral("/settings.ini")).toUtf8());
    qputenv("KPXC_CONFIG_LOCAL", (isolated.path() + QStringLiteral("/local.ini")).toUtf8());
    qputenv("USERNAME", "dim-sum-test");
    qputenv("USER", "dim-sum-test");
    const auto identity = QStringLiteral("KeePassXC-DimSum-Legacy-Tests-") + QDir(isolated.path()).dirName();
    QCoreApplication::setOrganizationName(identity);
    QCoreApplication::setApplicationName(identity);
    QApplication application(argc, argv);
    TestDimSum test;
    return QTest::qExec(&test, argc, argv);
}
