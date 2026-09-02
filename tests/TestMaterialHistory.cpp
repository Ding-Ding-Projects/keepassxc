#include "TestMaterialHistory.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialHistoryStore.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "keys/PasswordKey.h"
#include "config-keepassx-tests.h"
#include <QCheckBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSignalSpy>
#include <QLabel>
#include <QTimeZone>
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
#include <future>

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

    // The append-only banner is a real widget with the rule as its name.
    auto* banner = screen.findChild<QWidget*>(QStringLiteral("historyAppendOnlyBanner"));
    QVERIFY(banner);
    QVERIFY(banner->accessibleName().startsWith(QStringLiteral("History is append-only.")));

    // A kind badge reaches the row's accessible name, so it is not colour only.
    QVector<Revision> badged = revisions;
    badged[0].badge = QStringLiteral("EDIT");
    badged[0].hash = QStringLiteral("a1b2c3d");
    screen.setRevisions(badged);
    bool found = false;
    for (auto* widget : screen.findChildren<QWidget*>()) {
        if (widget->accessibleName().startsWith(QStringLiteral("EDIT: Edited Alpha"))) {
            found = true;
        }
    }
    QVERIFY(found);
}

void TestMaterialHistory::detailCardDescribesTheCurrentRevision()
{
    HistoryScreen screen;
    screen.resize(1200, 860);
    screen.show();
    Revision edited{QStringLiteral("entry-1"), QStringLiteral("edit"), QStringLiteral("Password changed"), QStringLiteral("2026-08-20 · entry"), RevisionTint::Accent, true, true, QStringLiteral("entry"), QDateTime(QDate(2026, 8, 20), QTime(2, 15, 11), QTimeZone::utc())};
    edited.badge = QStringLiteral("EDIT");
    edited.hash = QStringLiteral("9f2c1ab");
    edited.record = QStringLiteral("Discord — status bot");
    edited.detail = QStringLiteral("Password field replaced. Previous value retained in entry history.");
    edited.diff = {QStringLiteral("- password = <encrypted, previous>"), QStringLiteral("+ password = <encrypted, current>"), QStringLiteral("  last_accessed preserved")};
    Revision saved{QStringLiteral("save-1"), QStringLiteral("save"), QStringLiteral("Saved"), QStringLiteral("2026-08-19 · settings"), RevisionTint::Neutral, true, false, QStringLiteral("settings"), QDateTime::currentDateTime()};
    saved.badge = QStringLiteral("SETTINGS");
    saved.hash = QStringLiteral("5d42588");
    screen.setRevisions({edited, saved});
    screen.setState(HistoryScreen::State::Populated, QStringLiteral("2 revisions"));
    QCoreApplication::processEvents();

    // The newest revision is current and the card describes it.
    QVERIFY(screen.detailCardVisible());
    QCOMPARE(screen.currentRevisionId(), QStringLiteral("entry-1"));
    QCOMPARE(screen.findChild<QLabel*>(QStringLiteral("historyDetailBadge"))->text(), QStringLiteral("EDIT"));
    QCOMPARE(screen.findChild<QLabel*>(QStringLiteral("historyDetailHash"))->text(), QStringLiteral("9f2c1ab"));
    QCOMPARE(screen.findChild<QLabel*>(QStringLiteral("historyDetailRecord"))->text(), edited.record);
    QCOMPARE(screen.findChild<QLabel*>(QStringLiteral("historyDetailWhat"))->text(), edited.detail);
    QVERIFY(screen.findChild<QLabel*>(QStringLiteral("historyDetailWhen"))->text().startsWith(QStringLiteral("2026-08-20 02:15:11")));
    QCOMPARE(screen.findChild<QWidget*>(QStringLiteral("historyDetailDiff"))->findChildren<QLabel*>().size(), 6);
    // The rows keep their own actions only when the card is away.
    QVERIFY(screen.findChild<QAbstractButton*>(QStringLiteral("historyRestore_entry-1"))->isHidden());

    auto* restore = screen.findChild<QAbstractButton*>(QStringLiteral("historyDetailRestore"));
    QVERIFY(restore && restore->isEnabled());
    QCOMPARE(restore->text(), QStringLiteral("Restore 9f2c1ab"));
    QSignalSpy restoreSpy(&screen, &HistoryScreen::restoreRequested);
    restore->click();
    QCOMPARE(restoreSpy.count(), 1);
    QCOMPARE(restoreSpy.at(0).at(0).toString(), QStringLiteral("entry-1"));
    QSignalSpy diffSpy(&screen, &HistoryScreen::diffRequested);
    screen.findChild<QAbstractButton*>(QStringLiteral("historyDetailCompare"))->click();
    QCOMPARE(diffSpy.count(), 1);
    QSignalSpy exportSpy(&screen, &HistoryScreen::exportRequested);
    screen.findChild<QAbstractButton*>(QStringLiteral("historyDetailExport"))->click();
    QCOMPARE(exportSpy.at(0).at(0).toStringList(), QStringList{QStringLiteral("entry-1")});

    // A save record cannot be put back, and the card says so by disabling Restore.
    screen.setCurrentRevision(QStringLiteral("save-1"));
    QCOMPARE(screen.findChild<QLabel*>(QStringLiteral("historyDetailBadge"))->text(), QStringLiteral("SETTINGS"));
    QVERIFY(!restore->isEnabled());

    // Below the breakpoint the card gives way and the rows carry their actions.
    screen.resize(599, 800);
    QCoreApplication::processEvents();
    QVERIFY(!screen.detailCardVisible());
    QVERIFY(!screen.findChild<QAbstractButton*>(QStringLiteral("historyRestore_entry-1"))->isHidden());
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
    const QString encryptedPath = QDir(root.path()).filePath(QStringLiteral("private-name.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), encryptedPath));
    db->setFilePath(encryptedPath);
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

    const auto savedRevision = store.revisionsFor(db->filePath()).at(1);
    QVERIFY(!savedRevision.snapshotPath.isEmpty());
    QString snapshotError;
    const QByteArray snapshot = store.snapshot(savedRevision.id, &snapshotError);
    QVERIFY2(!snapshot.isEmpty(), qPrintable(snapshotError));
    QFile encryptedSource(encryptedPath);
    QVERIFY(encryptedSource.open(QIODevice::ReadOnly));
    QCOMPARE(snapshot, encryptedSource.readAll());
    QVERIFY(!snapshot.contains("secret password that must not persist"));
    QVERIFY(store.revisionsFor(db->filePath()).at(0).snapshotPath.isEmpty());

    QProcess gitShow;
    gitShow.start(gitExecutable, {QStringLiteral("-C"), repository, QStringLiteral("show"), QStringLiteral("HEAD:%1").arg(savedRevision.snapshotPath)});
    QVERIFY(gitShow.waitForFinished(10000));
    QCOMPARE(gitShow.exitCode(), 0);
    QCOMPARE(gitShow.readAllStandardOutput(), snapshot);

    const QString databaseRepository = store.databaseRepositoryPath(db->filePath());
    QVERIFY(QFileInfo::exists(QDir(databaseRepository).filePath(QStringLiteral(".git"))));
    QProcess databaseLog;
    databaseLog.start(gitExecutable,
                      {QStringLiteral("-C"),
                       databaseRepository,
                       QStringLiteral("rev-list"),
                       QStringLiteral("--count"),
                       QStringLiteral("HEAD")});
    QVERIFY(databaseLog.waitForFinished(10000));
    QCOMPARE(databaseLog.exitCode(), 0);
    QCOMPARE(QString::fromUtf8(databaseLog.readAllStandardOutput()).trimmed(), QStringLiteral("2"));

    QFile state(QDir(repository).filePath(QStringLiteral("revisions.json")));
    QVERIFY(state.open(QIODevice::ReadOnly));
    const QByteArray bytes = state.readAll();
    QVERIFY(!bytes.contains("private-name.kdbx"));
    QVERIFY(!bytes.contains("secret title"));
    QVERIFY(!bytes.contains("private title"));
    QVERIFY(!bytes.contains("secret password"));

    HistoryStore restarted(root.path(), gitExecutable);
    QCOMPARE(restarted.revisionsFor(db->filePath()).size(), 3);
    QCOMPARE(restarted.snapshot(savedRevision.id), snapshot);

    QFile corrupt(QDir(repository).filePath(savedRevision.snapshotPath));
    QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
    corrupt.write("not-kdbx");
    corrupt.close();
    QVERIFY(restarted.snapshot(savedRevision.id, &snapshotError).isEmpty());
    QVERIFY(!snapshotError.isEmpty());
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

void TestMaterialHistory::gitStoreSerializesConcurrentWriters()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY(!gitExecutable.isEmpty());
    const auto write = [storage = root.path(), gitExecutable](const QString& name) {
        auto db = QSharedPointer<Database>::create();
        const QString path = QDir(storage).filePath(name + QStringLiteral(".kdbx"));
        if (!QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), path)) return false;
        db->setFilePath(path);
        HistoryStore store(storage, gitExecutable);
        return store.recordSave(db);
    };
    auto first = std::async(std::launch::async, write, QStringLiteral("one"));
    auto second = std::async(std::launch::async, write, QStringLiteral("two"));
    QVERIFY(first.get());
    QVERIFY(second.get());
    HistoryStore readback(root.path(), gitExecutable);
    QCOMPARE(readback.revisions().size(), 2);

    QProcess log;
    log.start(gitExecutable,
              {QStringLiteral("-C"), QDir(root.path()).filePath(QStringLiteral("history/repository")),
               QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")});
    QVERIFY(log.waitForFinished(10000));
    QCOMPARE(QString::fromUtf8(log.readAllStandardOutput()).trimmed(), QStringLiteral("2"));
}

void TestMaterialHistory::restoresDeletedEntryFromPerDatabaseRepository()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY(!gitExecutable.isEmpty());
    const QString path = QDir(root.path()).filePath(QStringLiteral("restore.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), path));

    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create(QStringLiteral("a")));
    auto database = QSharedPointer<Database>::create();
    QString openError;
    QVERIFY2(database->open(path, key, &openError), qPrintable(openError));
    Entry* entry = database->rootGroup()->entriesRecursive(false).value(0);
    QVERIFY(entry);
    const QUuid deletedUuid = entry->uuid();

    HistoryStore store(root.path(), gitExecutable);
    QVERIFY(store.recordSave(database));
    entry->group()->removeEntry(entry);
    delete entry;
    database->addDeletedObject(deletedUuid);
    QString saveError;
    QVERIFY2(database->saveAs(path, Database::Atomic, {}, &saveError), qPrintable(saveError));
    QVERIFY(store.recordSave(database));
    const HistoryRevision deletion = store.revisionsFor(path).value(0);
    QCOMPARE(deletion.removed, 1);

    QString restoreError;
    QCOMPARE(store.restoreDeletedEntries(deletion.id, database, &restoreError), 1);
    QVERIFY2(restoreError.isEmpty(), qPrintable(restoreError));
    QVERIFY(database->rootGroup()->findEntryByUuid(deletedUuid));
    QVERIFY(!database->containsDeletedObject(deletedUuid));
}

QTEST_MAIN(TestMaterialHistory)
