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
    void gitStoreFailureDoesNotAdvanceFingerprint();
    void gitStoreMigratesLegacyOnce();
    void gitStoreSerializesConcurrentWriters();
    void restoresDeletedEntryFromPerDatabaseRepository();
    void feedBadgesTheCreatedStateAsCreate();
};
#endif
