#include "TestMaterialReports.h"
#include "gui/material/MaterialCard.h"
#include "gui/material/MaterialReportsScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

using namespace Material;

void TestMaterialReports::statesSelectionAndAccessibility()
{
    ReportsScreen screen;
    screen.resize(1200, 860);
    screen.show();
    screen.setState(ReportsScreen::State::Loading, QStringLiteral("Loading"));
    QCOMPARE(screen.state(), ReportsScreen::State::Loading);
    QVERIFY(screen.findChild<QProgressBar*>(QStringLiteral("reportsProgress"))->isVisible());

    screen.setStatCards({{QStringLiteral("Healthy"), QStringLiteral("4"), QStringLiteral("of 6"), Health::Ok}});
    screen.setHealthRows({{QStringLiteral("id-a"), QStringLiteral("warning"), QStringLiteral("Alpha"), QStringLiteral("Weak"), QStringLiteral("20 bits"), Health::Weak},
                          {QStringLiteral("id-b"), QStringLiteral("warning"), QStringLiteral("Beta"), QStringLiteral("Reused"), QStringLiteral("30 bits"), Health::Reused}});
    screen.setStatistics({{QStringLiteral("Entries"), QStringLiteral("6")}});
    screen.setState(ReportsScreen::State::Populated);
    QCOMPARE(screen.state(), ReportsScreen::State::Populated);

    const auto checks = screen.findChildren<QCheckBox*>();
    QCOMPARE(checks.size(), 2);
    QVERIFY(!checks.at(0)->accessibleName().isEmpty());
    QSignalSpy exportSpy(&screen, &ReportsScreen::bulkExportRequested);
    checks.at(0)->setChecked(true);
    QCOMPARE(screen.selectedFindingIds().size(), 1);
    auto buttons = screen.findChildren<QToolButton*>();
    QToolButton* bulk = nullptr;
    for (auto* button : buttons) if (button->text().contains(QStringLiteral("selected finding"))) bulk = button;
    QVERIFY(bulk && bulk->isEnabled());
    bulk->click();
    QCOMPARE(exportSpy.count(), 1);

    for (auto* widget : screen.findChildren<QWidget*>()) {
        if (widget->metaObject()->className() == QByteArrayLiteral("Material::StatTile")
            || widget->metaObject()->className() == QByteArrayLiteral("Material::HealthRowWidget")) {
            QVERIFY(!widget->accessibleName().isEmpty());
        }
    }
}

void TestMaterialReports::searchRegistrationAndResponsiveLayout()
{
    ReportsScreen screen;
    screen.setStatCards({{QStringLiteral("Healthy"), QStringLiteral("4"), QStringLiteral("of 6"), Health::Ok},
                         {QStringLiteral("Weak"), QStringLiteral("2"), QStringLiteral("of 6"), Health::Weak},
                         {QStringLiteral("Breaches"), QStringLiteral("—"), QStringLiteral("not checked"), Health::Unknown},
                         {QStringLiteral("Passkeys"), QStringLiteral("1"), QStringLiteral("stored"), Health::Unknown}});
    screen.setState(ReportsScreen::State::Populated);
    screen.resize(599, 800);
    screen.show();
    QTest::qWait(1);
    QCOMPARE(screen.searchBar()->searchId(), QStringLiteral("reports.findings"));
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("reports.findings")), screen.searchBar());
    emit screen.searchBar()->builderRequested();
    QCOMPARE(SearchRegistry::instance()->current(), screen.searchBar());
    auto* category = screen.findChild<QComboBox*>(QStringLiteral("reportsCategory"));
    QVERIFY(category);
    QVERIFY(!category->accessibleName().isEmpty());

    for (auto* card : screen.findChildren<Card*>()) {
        if (card->isVisible()) QVERIFY(card->geometry().right() <= card->parentWidget()->width());
    }
    screen.resize(1200, 800);
    QTest::qWait(1);
    for (auto* card : screen.findChildren<Card*>()) {
        if (card->isVisible()) QVERIFY(card->geometry().right() <= card->parentWidget()->width());
    }

    screen.setSearchValidation(false, QStringLiteral("Invalid regular expression"));
    QCOMPARE(screen.state(), ReportsScreen::State::Warning);
    QVERIFY(screen.searchBar()->lineEdit()->accessibleDescription().contains(QStringLiteral("Invalid")));
}

QTEST_MAIN(TestMaterialReports)
