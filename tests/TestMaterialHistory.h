#ifndef KEEPASSXC_TESTMATERIALHISTORY_H
#define KEEPASSXC_TESTMATERIALHISTORY_H
#include <QObject>
class TestMaterialHistory : public QObject
{
    Q_OBJECT
private slots:
    void surfaceStateFiltersAndSelection();
    void routeAndActionInventory();
};
#endif
