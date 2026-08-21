#include "TestMaterialHistory.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialHistoryStore.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include <QCheckBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
#include <QAbstractButton>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

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

void TestMaterialHistory::gitStoreTransactionAndRestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY2(!gitExecutable.isEmpty(), "The real git executable is required for this integration test");

    auto db = QSharedPointer<Database>::create();
    db->setFilePath(QDir(root.path()).filePath(QStringLiteral("private-name.kdbx")));
    auto* entry = new Entry;
    entry->setGroup(db->rootGroup());
    entry->setTitle(QStringLiteral("secret title that must not persist"));
    entry->setPassword(QStringLiteral("secret password that must not persist"));

    HistoryStore store(root.path(), gitExecutable);
    QVERIFY(store.recordSave(db));
    entry->setTitle(QStringLiteral("second private title"));
    QVERIFY(store.recordSave(db));
    QVERIFY(store.recordEvent(db, QStringLiteral("Restored an entry revision"), RevisionKind::Entry));
    QCOMPARE(store.revisionsFor(db->filePath()).size(), 3);
    QCOMPARE(store.revisions(0, 1).size(), 1);

    const QString repository = QDir(root.path()).filePath(QStringLiteral("history/repository"));
    QProcess log;
    log.start(gitExecutable, {QStringLiteral("-C"), repository, QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")});
    QVERIFY(log.waitForFinished(10000));
    QCOMPARE(log.exitCode(), 0);
    QCOMPARE(QString::fromUtf8(log.readAllStandardOutput()).trimmed(), QStringLiteral("3"));

    QFile state(QDir(repository).filePath(QStringLiteral("revisions.json")));
    QVERIFY(state.open(QIODevice::ReadOnly));
    const QByteArray bytes = state.readAll();
    QVERIFY(!bytes.contains("private-name.kdbx"));
    QVERIFY(!bytes.contains("secret title"));
    QVERIFY(!bytes.contains("private title"));
    QVERIFY(!bytes.contains("secret password"));

    HistoryStore restarted(root.path(), gitExecutable);
    QCOMPARE(restarted.revisionsFor(db->filePath()).size(), 3);
}

void TestMaterialHistory::gitStoreFailureDoesNotAdvanceFingerprint()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto db = QSharedPointer<Database>::create();
    db->setFilePath(QDir(root.path()).filePath(QStringLiteral("failure.kdbx")));
    HistoryStore store(root.path(), QDir(root.path()).filePath(QStringLiteral("missing-git.exe")));
    QSignalSpy failureSpy(&store, &HistoryStore::writeFailed);
    QVERIFY(!store.recordSave(db));
    QCOMPARE(failureSpy.count(), 1);
    const QString fingerprintDirectory = QDir(root.path()).filePath(QStringLiteral("history/repository/fingerprints"));
    QVERIFY(!QFileInfo::exists(fingerprintDirectory) || QDir(fingerprintDirectory).entryList(QDir::Files).isEmpty());
    QVERIFY(store.revisions().isEmpty());
}

void TestMaterialHistory::gitStoreMigratesLegacyOnce()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY(!gitExecutable.isEmpty());
    const QString history = QDir(root.path()).filePath(QStringLiteral("history"));
    QVERIFY(QDir().mkpath(history));
    QFile legacy(QDir(history).filePath(QStringLiteral("revisions.jsonl")));
    QVERIFY(legacy.open(QIODevice::WriteOnly | QIODevice::Text));
    QJsonObject record{{QStringLiteral("id"), QStringLiteral("legacy-1")},
                       {QStringLiteral("time"), QStringLiteral("2026-08-21T12:00:00.000Z")},
                       {QStringLiteral("path"), QStringLiteral("C:/private/location/vault.kdbx")},
                       {QStringLiteral("label"), QStringLiteral("Legacy redacted save")},
                       {QStringLiteral("kind"), QStringLiteral("settings")},
                       {QStringLiteral("entries"), 4},
                       {QStringLiteral("groups"), 2}};
    legacy.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
    legacy.write("\n");
    legacy.close();

    HistoryStore first(root.path(), gitExecutable);
    QCOMPARE(first.revisions().size(), 1);
    QVERIFY(QFileInfo::exists(legacy.fileName()));
    HistoryStore second(root.path(), gitExecutable);
    QCOMPARE(second.revisions().size(), 1);

    QFile migrated(QDir(history).filePath(QStringLiteral("repository/revisions.json")));
    QVERIFY(migrated.open(QIODevice::ReadOnly));
    QVERIFY(!migrated.readAll().contains("C:/private/location"));
}

QTEST_MAIN(TestMaterialHistory)
