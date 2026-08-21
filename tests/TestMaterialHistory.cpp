#include "TestMaterialHistory.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include <QCheckBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
#include <QAbstractButton>

using namespace Material;

void TestMaterialHistory::surfaceStateFiltersAndSelection()
{
    HistoryScreen screen;
    screen.resize(599, 800);
    screen.show();
    screen.setState(HistoryScreen::State::Loading, QStringLiteral("Loading"));
    QCOMPARE(screen.state(), HistoryScreen::State::Loading);
    const QVector<Revision> revisions{
        {QStringLiteral("entry-1"), QStringLiteral("edit"), QStringLiteral("Edited Alpha"), QStringLiteral("2026-08-20 · entry"), RevisionTint::Accent, true, true, QStringLiteral("entry"), QDateTime::currentDateTime()},
        {QStringLiteral("settings-1"), QStringLiteral("tune"), QStringLiteral("Changed settings"), QStringLiteral("2026-08-19 · settings"), RevisionTint::Neutral, true, false, QStringLiteral("settings"), QDateTime::currentDateTime().addDays(-1)},
        {QStringLiteral("restore-1"), QStringLiteral("restore"), QStringLiteral("Restored Alpha"), QStringLiteral("2026-08-18 · restore"), RevisionTint::Positive, false, false, QStringLiteral("restore"), QDateTime::currentDateTime().addDays(-2)}};
    screen.setRevisions(revisions);
    screen.setActionCounts({{QStringLiteral("entry"), 1}, {QStringLiteral("settings"), 1}, {QStringLiteral("restore"), 1}});
    screen.setState(HistoryScreen::State::Populated, QStringLiteral("3 revisions"));
    QCOMPARE(screen.state(), HistoryScreen::State::Populated);
    QCOMPARE(screen.findChildren<QCheckBox*>().size(), 3);
    auto* first = screen.findChildren<QCheckBox*>().at(0);
    QVERIFY(!first->accessibleName().isEmpty());
    first->setChecked(true);
    QCOMPARE(screen.selectedRevisionIds().size(), 1);
    auto* exportButton = screen.findChild<QToolButton*>(QStringLiteral("historyExportSelected"));
    QVERIFY(exportButton && exportButton->isEnabled());
    QSignalSpy exportSpy(&screen, &HistoryScreen::exportRequested);
    exportButton->click();
    QCOMPARE(exportSpy.count(), 1);
    QSignalSpy restoreSpy(&screen, &HistoryScreen::restoreRequested);
    auto* restoreButton = screen.findChild<QAbstractButton*>(QStringLiteral("historyRestore_entry-1"));
    QVERIFY(restoreButton);
    restoreButton->click();
    QCOMPARE(restoreSpy.count(), 1);
    QCOMPARE(restoreSpy.at(0).at(0).toString(), QStringLiteral("entry-1"));
    QVERIFY(screen.findChild<QDateEdit*>(QStringLiteral("historyFromDate")));
    QVERIFY(screen.findChild<QDateEdit*>(QStringLiteral("historyToDate")));
}

void TestMaterialHistory::routeAndActionInventory()
{
    HistoryScreen screen;
    QCOMPARE(screen.searchBar()->searchId(), QStringLiteral("history.revisions"));
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("history.revisions")), screen.searchBar());
    emit screen.searchBar()->builderRequested();
    QCOMPARE(SearchRegistry::instance()->current(), screen.searchBar());
    const QStringList expectedActions{QStringLiteral("entry"), QStringLiteral("settings"), QStringLiteral("restore")};
    screen.setActionCounts({{QStringLiteral("entry"), 2}, {QStringLiteral("settings"), 1}, {QStringLiteral("restore"), 1}});
    for (const auto& action : expectedActions) {
        auto* control = screen.findChild<QAbstractButton*>(QStringLiteral("historyAction_%1").arg(action));
        QVERIFY2(control, qPrintable(QStringLiteral("Missing exact history action control: %1").arg(action)));
        QVERIFY(!control->accessibleName().isEmpty() || !control->text().isEmpty());
    }
    const QList<HistoryScreen::State> states{HistoryScreen::State::Empty,
                                             HistoryScreen::State::Loading,
                                             HistoryScreen::State::Populated,
                                             HistoryScreen::State::Progress,
                                             HistoryScreen::State::Warning,
                                             HistoryScreen::State::Error};
    for (const auto state : states) {
        screen.setState(state, QStringLiteral("state"));
        QCOMPARE(screen.state(), state);
    }
    screen.searchBar()->setRegexEnabled(true);
    screen.searchBar()->setText(QStringLiteral("["));
    screen.searchBar()->lineEdit()->setAccessibleDescription(QStringLiteral("Invalid regular expression"));
    QVERIFY(screen.searchBar()->lineEdit()->accessibleDescription().contains(QStringLiteral("Invalid")));
}

QTEST_MAIN(TestMaterialHistory)
