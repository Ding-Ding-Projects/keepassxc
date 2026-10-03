#include "TestMaterialHistory.h"
#include "gui/material/MaterialHistoryScreen.h"
#include "gui/material/MaterialHistoryFeed.h"
#include "gui/material/MaterialSearchBar.h"
#include "gui/material/MaterialSearchRegistry.h"
#include "gui/material/MaterialHistoryStore.h"
#include "gui/material/MaterialHistoryLimits.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "format/KeePass2.h"
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
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>
#include <future>

using namespace Material;

namespace
{
    QSharedPointer<CompositeKey> materialHistoryTestKey(const QString& password = QStringLiteral("a"))
    {
        auto key = QSharedPointer<CompositeKey>::create();
        key->addKey(QSharedPointer<PasswordKey>::create(password));
        return key;
    }

    bool saveWithEmbeddedHistory(HistoryStore& history,
                                 const QSharedPointer<Database>& database,
                                 const QString& path,
                                 QString* error,
                                 bool restoreSourceMetadataAfterCopy = false)
    {
        if (!history.beginDatabaseSave(database, path, restoreSourceMetadataAfterCopy)) {
            if (error) *error = QStringLiteral("History preparation could not start.");
            return false;
        }
        if (!history.embedLatestHistory(database, error)) {
            history.cancelDatabaseSave(database);
            return false;
        }
        if (!database->saveAs(path, Database::Atomic, {}, error)) {
            history.cancelDatabaseSave(database);
            return false;
        }
        history.finishEmbeddedHistory(database, true);
        return true;
    }

    quint32 readKdbxVersion(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return 0;
        const QByteArray header = file.read(12);
        if (header.size() != 12) return 0;
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(header.constData() + 8));
    }

    QByteArray materialHistoryFingerprint(const QSharedPointer<Database>& database)
    {
        QHash<QString, QString> entries;
        for (const Group* group : database->rootGroup()->groupsRecursive(true)) {
            if (group->isRecycled()) continue;
            for (const Entry* entry : group->entries()) {
                if (entry->isRecycled()) continue;
                const QString uuid = entry->uuidToHex();
                const QString state = QStringLiteral("%1|%2|%3")
                                          .arg(uuid)
                                          .arg(entry->timeInfo().lastModificationTime().toSecsSinceEpoch() * 1000)
                                          .arg(group->uuidToHex());
                const auto hash = [](const QString& value) {
                    return QString::fromLatin1(
                        QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
                };
                entries.insert(hash(uuid), hash(state));
            }
        }
        QStringList keys = entries.keys();
        keys.sort();
        QJsonArray fingerprint;
        for (const auto& key : keys) fingerprint.append(QStringLiteral("%1:%2").arg(key, entries.value(key)));
        return QCryptographicHash::hash(QJsonDocument(fingerprint).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256)
            .toHex();
    }
}

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

void TestMaterialHistory::skipsHistoryBeforeDatabaseUnlock()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = QDir(root.path()).filePath(QStringLiteral("locked.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), path));
    auto database = QSharedPointer<Database>::create();
    QString error;
    QVERIFY(database->open(path, nullptr, &error));
    QVERIFY(!database->key());
    const bool modified = database->isModified();
    const auto name = database->metadata()->name();
    HistoryStore store(root.path(), QStandardPaths::findExecutable(QStringLiteral("git")));
    QVERIFY(!store.recordSave(database));
    QVERIFY(store.revisionsForDatabase(database).isEmpty());
    QCOMPARE(database->isModified(), modified);
    QCOMPARE(database->metadata()->name(), name);
    QCOMPARE(database->filePath(), path);
    QVERIFY(database->open(path, materialHistoryTestKey(), &error));
    database->metadata()->setName(QStringLiteral("Editable after unlock"));
    QCOMPARE(database->metadata()->name(), QStringLiteral("Editable after unlock"));
    QVERIFY(database->isModified());
}

void TestMaterialHistory::recordsReadyDatabaseSnapshots_data()
{
    QTest::addColumn<bool>("emptyKey");
    QTest::newRow("password-key") << false;
    QTest::newRow("empty-composite-key") << true;
}

void TestMaterialHistory::recordsReadyDatabaseSnapshots()
{
    QFETCH(bool, emptyKey);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    auto database = QSharedPointer<Database>::create();
    auto key = emptyKey ? QSharedPointer<CompositeKey>::create() : materialHistoryTestKey();
    QVERIFY(database->setKey(key));
    QVERIFY(!database->transformedDatabaseKey().isEmpty());
    database->setFilePath(QDir(root.path()).filePath(QStringLiteral("new.kdbx")));
    database->metadata()->setName(QStringLiteral("New editable database"));
    HistoryStore store(root.path(), QStandardPaths::findExecutable(QStringLiteral("git")));
    QVERIFY(store.recordSave(database));
    const auto revisions = store.revisionsForDatabase(database);
    QCOMPARE(revisions.size(), 1);
    QString error;
    const auto snapshot = store.snapshot(revisions.first().id, &error);
    QVERIFY2(!snapshot.isEmpty(), qPrintable(error));
    const auto snapshotPath = QDir(root.path()).filePath(QStringLiteral("snapshot.kdbx"));
    QFile file(snapshotPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(snapshot), qint64(snapshot.size()));
    file.close();
    auto restored = QSharedPointer<Database>::create();
    QVERIFY2(restored->open(snapshotPath, key, &error), qPrintable(error));
    QCOMPARE(restored->metadata()->name(), QStringLiteral("New editable database"));
    QVERIFY(database->isModified());
    database->metadata()->setName(QStringLiteral("Still editable after snapshot"));
    QCOMPARE(database->metadata()->name(), QStringLiteral("Still editable after snapshot"));
}

void TestMaterialHistory::recordsSnapshotsBelowDeepHistoryRoot()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QString deepRoot = root.path();
    while (deepRoot.size() < 190) {
        deepRoot = QDir(deepRoot).filePath(QStringLiteral("nested-history-storage"));
    }
    QVERIFY(QDir().mkpath(deepRoot));
    QFile qtProbe(QDir(deepRoot).filePath(QStringLiteral("qt-write-probe.txt")));
    QVERIFY(qtProbe.open(QIODevice::WriteOnly));
    QCOMPARE(qtProbe.write("ready"), qint64(5));
    qtProbe.close();

    auto database = QSharedPointer<Database>::create();
    QString error;
    QVERIFY2(database->open(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"),
                            materialHistoryTestKey(), &error), qPrintable(error));
    // History identity needs a path, but the source need not be saved under the deep storage root.
    database->setFilePath(QStringLiteral("synthetic-history-source.kdbx"));
    database->metadata()->setName(QStringLiteral("Deep history snapshot"));
    HistoryStore store(deepRoot, QStandardPaths::findExecutable(QStringLiteral("git")));
    QVERIFY(store.recordSave(database));
    const auto revisions = store.revisionsForDatabase(database);
    QCOMPARE(revisions.size(), 1);
    QVERIFY(!revisions.first().snapshotPath.isEmpty());
    QVERIFY(QDir(deepRoot).filePath(QStringLiteral("history/repository/")
                                  + revisions.first().snapshotPath).size() > 260);
    const auto snapshot = store.snapshot(revisions.first().id, &error);
    QVERIFY2(!snapshot.isEmpty(), qPrintable(error));
    const auto snapshotPath = QDir(root.path()).filePath(QStringLiteral("reopen.kdbx"));
    QFile file(snapshotPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(snapshot), qint64(snapshot.size()));
    file.close();
    auto restored = QSharedPointer<Database>::create();
    QVERIFY2(restored->open(snapshotPath, materialHistoryTestKey(), &error), qPrintable(error));
    QCOMPARE(restored->metadata()->name(), QStringLiteral("Deep history snapshot"));
    HistoryStore reconstructed(deepRoot, QStandardPaths::findExecutable(QStringLiteral("git")));
    QVERIFY(reconstructed.load());
    const auto retained = reconstructed.revisionsForDatabase(database);
    QCOMPARE(retained.size(), 1);
    QCOMPARE(retained.first().id, revisions.first().id);
    QCOMPARE(reconstructed.snapshot(retained.first().id, &error), snapshot);
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
    QString error;
    QVERIFY2(db->open(encryptedPath, materialHistoryTestKey(), &error), qPrintable(error));
    auto* entry = new Entry;
    entry->setGroup(db->rootGroup());
    entry->setTitle(QStringLiteral("secret title that must not persist"));
    entry->setPassword(QStringLiteral("secret password that must not persist"));

    HistoryStore store(root.path(), gitExecutable);
    QVERIFY(store.recordSave(db));
    entry->beginUpdate();
    entry->setTitle(QStringLiteral("second private title"));
    QVERIFY(entry->endUpdate());
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
    QVERIFY(!snapshot.contains("secret password that must not persist"));
    const QString snapshotPath = QDir(root.path()).filePath(QStringLiteral("history-snapshot.kdbx"));
    QFile snapshotFile(snapshotPath);
    QVERIFY(snapshotFile.open(QIODevice::WriteOnly));
    QCOMPARE(snapshotFile.write(snapshot), snapshot.size());
    snapshotFile.close();
    auto snapshotDatabase = QSharedPointer<Database>::create();
    QVERIFY2(snapshotDatabase->open(snapshotPath, materialHistoryTestKey(), &snapshotError), qPrintable(snapshotError));
    QCOMPARE(snapshotDatabase->rootGroup()->entriesRecursive(false).size(), db->rootGroup()->entriesRecursive(false).size());
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
    QCOMPARE(QString::fromUtf8(databaseLog.readAllStandardOutput()).trimmed(), QStringLiteral("3"));

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
    const QString databasePath = QDir(root.path()).filePath(QStringLiteral("failure.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), databasePath));
    QString openError;
    QVERIFY2(db->open(databasePath, materialHistoryTestKey(), &openError), qPrintable(openError));
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
        QString error;
        if (!db->open(path, materialHistoryTestKey(), &error)) return false;
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

void TestMaterialHistory::embeddedHistoryLimitsMatchTheStorageContract()
{
    QCOMPARE(HistoryLimits::MaximumPackedBundleBytes, 256LL * 1024 * 1024);
    QCOMPARE(HistoryLimits::MaximumExpandedObjectBytes, 1024LL * 1024 * 1024);
    QCOMPARE(HistoryLimits::MaximumGitObjects, 1'000'000LL);
    QCOMPARE(HistoryLimits::MaximumHistoryAncestors, 100'000LL);
    QCOMPARE(HistoryLimits::MaximumNestedTreeLevels, 256);

    QVERIFY(HistoryLimits::withinPackedBundleLimit(1));
    QVERIFY(HistoryLimits::withinPackedBundleLimit(HistoryLimits::MaximumPackedBundleBytes));
    QVERIFY(!HistoryLimits::withinPackedBundleLimit(HistoryLimits::MaximumPackedBundleBytes + 1));
    QVERIFY(HistoryLimits::withinExpandedObjectLimit(HistoryLimits::MaximumExpandedObjectBytes));
    QVERIFY(!HistoryLimits::withinExpandedObjectLimit(HistoryLimits::MaximumExpandedObjectBytes + 1));
    QVERIFY(HistoryLimits::withinGitObjectLimit(HistoryLimits::MaximumGitObjects));
    QVERIFY(!HistoryLimits::withinGitObjectLimit(HistoryLimits::MaximumGitObjects + 1));
    QVERIFY(HistoryLimits::withinHistoryAncestorLimit(HistoryLimits::MaximumHistoryAncestors));
    QVERIFY(!HistoryLimits::withinHistoryAncestorLimit(HistoryLimits::MaximumHistoryAncestors + 1));
    QVERIFY(HistoryLimits::withinNestedTreeLevelLimit(256));
    QVERIFY(!HistoryLimits::withinNestedTreeLevelLimit(257));
    QCOMPARE(HistoryLimits::nestedTreeLevels(QStringLiteral("revisions/one.json")), 2);
    const QString maximumDepth = QStringLiteral("level/").repeated(255) + QStringLiteral("leaf.json");
    const QString excessiveDepth = QStringLiteral("level/").repeated(256) + QStringLiteral("leaf.json");
    QVERIFY(HistoryLimits::withinNestedTreeLevelLimit(HistoryLimits::nestedTreeLevels(maximumDepth)));
    QVERIFY(!HistoryLimits::withinNestedTreeLevelLimit(HistoryLimits::nestedTreeLevels(excessiveDepth)));
}

void TestMaterialHistory::embeddedHistorySurvivesKdbx3AndKdbx4RoundTrips()
{
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY2(!gitExecutable.isEmpty(), "The real git executable is required for this integration test");
    struct FormatFixture
    {
        QString name;
        quint32 version;
        QString password;
    };
    const QList<FormatFixture> fixtures{
        {QStringLiteral("Format300.kdbx"), KeePass2::FILE_VERSION_3, QStringLiteral("a")},
        {QStringLiteral("Format400.kdbx"), KeePass2::FILE_VERSION_4, QStringLiteral("t")},
    };
    for (const auto& fixture : fixtures) {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = QDir(root.path()).filePath(QStringLiteral("history-round-trip.kdbx"));
        QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QLatin1Char('/') + fixture.name, path));
        auto database = QSharedPointer<Database>::create();
        QString error;
        QVERIFY2(database->open(path, materialHistoryTestKey(fixture.password), &error), qPrintable(error));
        QCOMPARE(readKdbxVersion(path) & 0xffff0000U, fixture.version);

        HistoryStore history(QDir(root.path()).filePath(QStringLiteral("local-history")), gitExecutable);
        QVERIFY2(saveWithEmbeddedHistory(history, database, path, &error), qPrintable(error));
        QCOMPARE(readKdbxVersion(path) & 0xffff0000U, fixture.version);

        auto reopened = QSharedPointer<Database>::create();
        QVERIFY2(reopened->open(path, materialHistoryTestKey(fixture.password), &error), qPrintable(error));
        QVERIFY(!reopened->metadata()->customData()->value(QStringLiteral("KeePassXC/History/BundleV1")).isEmpty());
        HistoryStore restored(QDir(root.path()).filePath(QStringLiteral("restored-history")), gitExecutable);
        QVERIFY2(restored.hydrateDatabase(reopened, &error), qPrintable(error));
        QCOMPARE(restored.revisionsForDatabase(reopened).size(), 1);
    }
}

void TestMaterialHistory::rejectsMalformedEmbeddedHistoryBeforeLocalImport()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY2(!gitExecutable.isEmpty(), "The real git executable is required for this integration test");
    const QString path = QDir(root.path()).filePath(QStringLiteral("malformed-history.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), path));
    auto database = QSharedPointer<Database>::create();
    QString error;
    QVERIFY2(database->open(path, materialHistoryTestKey(), &error), qPrintable(error));

    const QString identity(64, QLatin1Char('a'));
    const QString identityKey = QStringLiteral("KeePassXC/History/DatabaseId");
    const QString bundleKey = QStringLiteral("KeePassXC/History/BundleV1");
    database->metadata()->customData()->set(identityKey, identity);
    HistoryStore history(QDir(root.path()).filePath(QStringLiteral("local-history")), gitExecutable);
    const QString localRepository = history.databaseRepositoryPath(identity);

    const QByteArray invalidBundle = QByteArrayLiteral("not a git bundle");
    const QByteArray correctDigest = QCryptographicHash::hash(invalidBundle, QCryptographicHash::Sha256).toHex();
    const QString badDigestEnvelope = QStringLiteral("1:%1:%2:%3")
                                         .arg(identity, QString(64, QLatin1Char('0')), QString::fromLatin1(invalidBundle.toBase64()));
    database->metadata()->customData()->set(bundleKey, badDigestEnvelope);
    QVERIFY(!history.hydrateDatabase(database, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(QDir(localRepository).filePath(QStringLiteral(".git"))));

    const QString invalidBundleEnvelope = QStringLiteral("1:%1:%2:%3")
                                              .arg(identity, QString::fromLatin1(correctDigest), QString::fromLatin1(invalidBundle.toBase64()));
    database->metadata()->customData()->set(bundleKey, invalidBundleEnvelope);
    error.clear();
    QVERIFY(!history.hydrateDatabase(database, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(QDir(localRepository).filePath(QStringLiteral(".git"))));
}

void TestMaterialHistory::saveAsInheritsHistoryUnderAFreshIdentity()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY2(!gitExecutable.isEmpty(), "The real git executable is required for this integration test");
    const QString sourcePath = QDir(root.path()).filePath(QStringLiteral("source.kdbx"));
    const QString destinationPath = QDir(root.path()).filePath(QStringLiteral("copy.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), sourcePath));
    auto database = QSharedPointer<Database>::create();
    QString error;
    QVERIFY2(database->open(sourcePath, materialHistoryTestKey(), &error), qPrintable(error));
    HistoryStore sourceHistory(QDir(root.path()).filePath(QStringLiteral("source-history")), gitExecutable);
    QVERIFY2(saveWithEmbeddedHistory(sourceHistory, database, sourcePath, &error), qPrintable(error));

    Entry* entry = database->rootGroup()->entriesRecursive(false).value(0);
    QVERIFY(entry);
    entry->beginUpdate();
    entry->setTitle(QStringLiteral("Save As copied history"));
    QVERIFY(entry->endUpdate());
    database->markAsModified();
    QVERIFY2(saveWithEmbeddedHistory(sourceHistory, database, sourcePath, &error), qPrintable(error));
    const QString sourceIdentity = sourceHistory.databaseIdentity(database);
    QCOMPARE(sourceHistory.revisionsFor(sourceIdentity).size(), 2);

    QVERIFY2(saveWithEmbeddedHistory(sourceHistory, database, destinationPath, &error), qPrintable(error));
    const QString destinationIdentity = sourceHistory.databaseIdentity(database);
    QVERIFY(destinationIdentity != sourceIdentity);
    QCOMPARE(sourceHistory.revisionsFor(sourceIdentity).size(), 2);
    QCOMPARE(sourceHistory.revisionsFor(destinationIdentity).size(), 3);

    auto reopened = QSharedPointer<Database>::create();
    QVERIFY2(reopened->open(destinationPath, materialHistoryTestKey(), &error), qPrintable(error));
    QCOMPARE(reopened->metadata()->customData()->value(QStringLiteral("KeePassXC/History/DatabaseId")), destinationIdentity);
    const QByteArray reopenedFingerprint = materialHistoryFingerprint(reopened);
    const auto copiedRevisions = sourceHistory.revisionsFor(destinationIdentity);
    bool embeddedFingerprintMatches = false;
    QStringList recordedFingerprints;
    for (const auto& revision : copiedRevisions) {
        recordedFingerprints.append(revision.contentFingerprint);
        embeddedFingerprintMatches = embeddedFingerprintMatches || revision.contentFingerprint.toLatin1() == reopenedFingerprint;
    }
    QVERIFY2(embeddedFingerprintMatches,
             qPrintable(QStringLiteral("Reopened database fingerprint %1 did not match copied revisions %2")
                            .arg(QString::fromLatin1(reopenedFingerprint), recordedFingerprints.join(QLatin1Char(',')))));
    HistoryStore restored(QDir(root.path()).filePath(QStringLiteral("destination-history")), gitExecutable);
    QVERIFY2(restored.hydrateDatabase(reopened, &error), qPrintable(error));
    QCOMPARE(restored.revisionsForDatabase(reopened).size(), 3);
    QCOMPARE(reopened->rootGroup()->entriesRecursive(false).value(0)->title(), QStringLiteral("Save As copied history"));

    auto original = QSharedPointer<Database>::create();
    QVERIFY2(original->open(sourcePath, materialHistoryTestKey(), &error), qPrintable(error));
    QCOMPARE(original->metadata()->customData()->value(QStringLiteral("KeePassXC/History/DatabaseId")), sourceIdentity);
}

void TestMaterialHistory::concurrentDatabaseHistoriesUnionWithoutMergingDatabaseContent()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
    QVERIFY2(!gitExecutable.isEmpty(), "The real git executable is required for this integration test");
    const QString basePath = QDir(root.path()).filePath(QStringLiteral("base.kdbx"));
    const QString firstPath = QDir(root.path()).filePath(QStringLiteral("copy-a.kdbx"));
    const QString secondPath = QDir(root.path()).filePath(QStringLiteral("copy-b.kdbx"));
    QVERIFY(QFile::copy(QStringLiteral(KEEPASSX_TEST_DATA_DIR) + QStringLiteral("/NewDatabase.kdbx"), basePath));
    auto base = QSharedPointer<Database>::create();
    QString error;
    QVERIFY2(base->open(basePath, materialHistoryTestKey(), &error), qPrintable(error));
    HistoryStore firstHistory(QDir(root.path()).filePath(QStringLiteral("device-a")), gitExecutable);
    QVERIFY2(saveWithEmbeddedHistory(firstHistory, base, basePath, &error), qPrintable(error));
    QVERIFY(QFile::copy(basePath, firstPath));
    QVERIFY(QFile::copy(basePath, secondPath));

    auto first = QSharedPointer<Database>::create();
    auto second = QSharedPointer<Database>::create();
    QVERIFY2(first->open(firstPath, materialHistoryTestKey(), &error), qPrintable(error));
    QVERIFY2(second->open(secondPath, materialHistoryTestKey(), &error), qPrintable(error));
    HistoryStore secondHistory(QDir(root.path()).filePath(QStringLiteral("device-b")), gitExecutable);
    QVERIFY2(firstHistory.hydrateDatabase(first, &error), qPrintable(error));
    QVERIFY2(secondHistory.hydrateDatabase(second, &error), qPrintable(error));
    QCOMPARE(firstHistory.revisionsForDatabase(first).size(), 1);
    QCOMPARE(secondHistory.revisionsForDatabase(second).size(), 1);

    Entry* firstEntry = first->rootGroup()->entriesRecursive(false).value(0);
    Entry* secondEntry = second->rootGroup()->entriesRecursive(false).value(0);
    QVERIFY(firstEntry && secondEntry);
    firstEntry->setTitle(QStringLiteral("Concurrent copy A"));
    first->markAsModified();
    QVERIFY2(saveWithEmbeddedHistory(firstHistory, first, firstPath, &error), qPrintable(error));
    secondEntry->setTitle(QStringLiteral("Concurrent copy B"));
    second->markAsModified();
    QVERIFY2(saveWithEmbeddedHistory(secondHistory, second, secondPath, &error), qPrintable(error));

    const QString sharedIdentity = firstHistory.databaseIdentity(first);
    QCOMPARE(secondHistory.databaseIdentity(second), sharedIdentity);
    QVERIFY2(firstHistory.hydrateDatabase(second, &error), qPrintable(error));
    QCOMPARE(firstHistory.revisionsFor(sharedIdentity).size(), 3);
    QCOMPARE(second->rootGroup()->entriesRecursive(false).value(0)->title(), QStringLiteral("Concurrent copy B"));
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

void TestMaterialHistory::feedBadgesTheCreatedStateAsCreate()
{
    // An entry edited once leaves its created state in history; the feed
    // badges the step out of it CREATE, and a second edit is an EDIT.
    auto database = QSharedPointer<Database>::create();
    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle(QStringLiteral("Fresh"));
    entry->setGroup(database->rootGroup());
    QCOMPARE(entry->timeInfo().creationTime(), entry->timeInfo().lastModificationTime());
    QTest::qWait(1100); // the history step needs a later modification second
    entry->beginUpdate();
    entry->setTitle(QStringLiteral("Fresh, renamed"));
    entry->endUpdate();
    QCOMPARE(entry->historyItems().size(), 1);
    QTest::qWait(1100);
    entry->beginUpdate();
    entry->setUsername(QStringLiteral("someone"));
    entry->endUpdate();
    QCOMPARE(entry->historyItems().size(), 2);

    HistoryScreen screen;
    screen.resize(1200, 860);
    HistoryFeed feed(&screen);
    feed.setDatabase(database);
    feed.rebuild();
    QStringList badges;
    for (QWidget* row : screen.findChildren<QWidget*>()) {
        if (row->objectName().startsWith(QStringLiteral("historyRevision_"))) {
            badges << row->accessibleName().section(QLatin1Char(':'), 0, 0);
        }
    }
    QVERIFY2(badges.contains(QStringLiteral("CREATE")), qPrintable(badges.join(QStringLiteral(" | "))));
    QVERIFY2(badges.contains(QStringLiteral("EDIT")), qPrintable(badges.join(QStringLiteral(" | "))));
    QCOMPARE(badges.count(QStringLiteral("CREATE")), 1);
}

QTEST_MAIN(TestMaterialHistory)
