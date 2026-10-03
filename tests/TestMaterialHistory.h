#ifndef KEEPASSXC_TESTMATERIALHISTORY_H
#define KEEPASSXC_TESTMATERIALHISTORY_H
#include <QObject>
class TestMaterialHistory : public QObject
{
    Q_OBJECT
private slots:
    void surfaceStateFiltersAndSelection();
    void detailCardDescribesTheCurrentRevision();
    void routeAndActionInventory();
    void gitStoreTransactionAndRestart();
    void skipsHistoryBeforeDatabaseUnlock();
    void recordsReadyDatabaseSnapshots_data();
    void recordsReadyDatabaseSnapshots();
    void gitStoreFailureDoesNotAdvanceFingerprint();
    void gitStoreMigratesLegacyOnce();
    void gitStoreSerializesConcurrentWriters();
    void embeddedHistoryLimitsMatchTheStorageContract();
    void embeddedHistorySurvivesKdbx3AndKdbx4RoundTrips();
    void rejectsMalformedEmbeddedHistoryBeforeLocalImport();
    void saveAsInheritsHistoryUnderAFreshIdentity();
    void concurrentDatabaseHistoriesUnionWithoutMergingDatabaseContent();
    void restoresDeletedEntryFromPerDatabaseRepository();
    void feedBadgesTheCreatedStateAsCreate();
};
#endif
