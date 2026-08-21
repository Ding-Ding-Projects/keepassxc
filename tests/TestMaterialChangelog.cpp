#include "TestMaterialChangelog.h"
#include "gui/material/MaterialChangelogFeed.h"
#include "gui/material/MaterialChangelogScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"

#include <QAbstractButton>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QTest>

using namespace Material;

void TestMaterialChangelog::parsesAuthoritativeFieldsWithoutInventingCommits()
{
    const QString markdown = QStringLiteral("## 2.0.0 (2026-08-20)\n### Added\n- **Real** feature [#12]\n\n## 1.0.0 (2025-01-02)\n- Earlier release\n");
    const auto releases = ChangelogFeed::parse(markdown);
    QCOMPARE(releases.size(), 2);
    QCOMPARE(releases.at(0).version, QStringLiteral("2.0.0"));
    QCOMPARE(releases.at(0).date, QStringLiteral("2026-08-20"));
    QVERIFY(!releases.at(0).items.isEmpty());
    QVERIFY(releases.at(0).commitSha.isEmpty());
    QVERIFY(!releases.at(0).commitAvailable);
}

void TestMaterialChangelog::routeActionsDatesAndFiltering()
{
    ChangelogScreen screen;
    QCOMPARE(screen.searchBar()->searchId(), QStringLiteral("changelog.entries"));
    QCOMPARE(SearchRegistry::instance()->bar(QStringLiteral("changelog.entries")), screen.searchBar());
    const QStringList actions{QStringLiteral("changelogCopyFiltered")};
    for (const auto& action : actions) QVERIFY(screen.findChild<QAbstractButton*>(action));
    QVERIFY(screen.findChild<QDateEdit*>(QStringLiteral("changelogFromDate")));
    QVERIFY(screen.findChild<QDateEdit*>(QStringLiteral("changelogToDate")));

    Release first; first.version = QStringLiteral("2.0.0"); first.date = QStringLiteral("2026-08-20"); first.items.append({QStringLiteral("Added"), QStringLiteral("Alpha markdown **bold**"), PillKind::Good});
    Release second; second.version = QStringLiteral("1.0.0"); second.date = QStringLiteral("2024-01-01"); second.items.append({QStringLiteral("Fixed"), QStringLiteral("Beta"), PillKind::Bad});
    screen.setReleases({first, second});
    QCOMPARE(screen.filteredReleases().size(), 2);
    screen.searchBar()->setText(QStringLiteral("Alpha"));
    QCOMPARE(screen.filteredReleases().size(), 1);
    auto commitLabels = screen.findChildren<QLabel*>();
    int unavailable = 0;
    for (auto* label : commitLabels) if (label->objectName().startsWith(QStringLiteral("changelogCommit_")) && label->text().contains(QStringLiteral("unavailable"), Qt::CaseInsensitive)) ++unavailable;
    QCOMPARE(unavailable, 1);
    screen.searchBar()->setRegexEnabled(true);
    screen.searchBar()->setText(QStringLiteral("["));
    QVERIFY(screen.filteredReleases().isEmpty());
    QVERIFY(screen.searchBar()->lineEdit()->accessibleDescription().contains(QStringLiteral("Invalid")));
}

QTEST_MAIN(TestMaterialChangelog)
